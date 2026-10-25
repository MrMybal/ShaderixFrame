#include "media/ExportJobs.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <mfapi.h>

static int checks = 0;
static void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
    ++checks;
    std::cout << message << std::endl;
}
static sf::ExportInfo wait(sf::ExportJobs &jobs, sf::Id id) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    float previous = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        auto info = jobs.status(id);
        if (info.progress < previous || info.progress < 0 || info.progress > 1)
            throw std::runtime_error("Non-monotonic export progress");
        previous = info.progress;
        if (!info.active())
            return info;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("Background export timed out");
}
static void protect(const std::string &path) {
    std::ofstream file(sx::Utf8Path(path), std::ios::binary);
    file << "Existing destination";
}
static bool protectedFile(const std::string &path) {
    std::string content;
    return sx::ReadTextFile(path, content) && content == "Existing destination";
}
int main(int argc, char **argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);
    int result = 0;
    try {
        auto root = sx::Utf8Path(argv[1]);
        auto directory = std::filesystem::current_path() / "test-output/background";
        std::filesystem::create_directories(directory);
        auto path = [&](const char *file) { return sx::Narrow((directory / file).wstring()); };
        auto p = sf::Project::demo();
        p.width = 320;
        p.height = 180;
        p.duration = .4;
        p.name = "Immutable snapshot";
        auto video = p.tracks[0].clips[0].id;
        p.findClip(video)->effects = {sf::Effect{}};
        p.findClip(video)->effects[0].id = "snapshot-test";
        sf::EffectLibrary library;
        sf::EffectDefinition red;
        red.id = "snapshot-test";
        red.code = "float4 Effect(EffectContext ctx){return float4(.8,.1,.05,1);}";
        library.definitions.push_back(red);
        sf::Clip title;
        title.id = p.allocate();
        title.title.enabled = true;
        title.title.text = "Été · snapshot";
        title.duration = .4;
        title.params["font_size"] = {20};
        p.addTrack(sf::TrackKind::Video).clips.push_back(title);
        // Keep the title above the generator in the composition.
        std::swap(p.tracks.front(), p.tracks.back());
        sf::Clip audio;
        audio.id = p.allocate();
        audio.duration = .4;
        audio.source = sx::Narrow((root / "examples/firstlight.wav").wstring());
        audio.effects = {sf::instantiateAudioEffect("cydaw.eq-three")};
        p.tracks[1].clips.push_back(audio);
        p.validate();
        sf::ExportJobs jobs;
        auto untouched = p.json().Dump(0);
        auto started = std::chrono::steady_clock::now();
        auto id = jobs.start(p, library, sf::ExportKind::Video, path("snapshot.mp4"));
        require(std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() < 1,
                "Starting a background export returns without waiting for the encoder");
        auto metadata = jobs.status(id);
        require(metadata.project == "Immutable snapshot" && metadata.width == 320 &&
                    metadata.duration == .4 && p.json().Dump(0) == untouched,
                "Export metadata describes the immutable sequence and leaves the document untouched");
        p.name = "Edited during rendering";
        p.duration = .1;
        p.width = 640;
        p.findClip(video)->params["opacity"] = {0};
        library.definitions[0].code = "float4 Effect(EffectContext ctx){return float4(0,0,1,1);}";
        sf::MediaCache previewMedia;
        sf::Renderer preview;
        preview.initialize();
        preview.library = library;
        preview.render(p, previewMedia, .05, 320, 180);
        auto rendered = preview.readback();
        auto done = wait(jobs, id);
        require(done.state == sf::ExportState::Completed && done.progress == 1 && done.error.empty() &&
                    done.elapsed > 0,
                "Worker with its own COM/GPU runtime completes the export");
        sf::MediaAsset movie;
        movie.open(path("snapshot.mp4"));
        auto image = movie.at(.1);
        size_t pixel = (20 * 320 + 20) * 4;
        size_t bottom = (size_t(movie.frame.height - 1) * movie.frame.width + 20) * 4;
        require(movie.frame.height == 180 && image.pixels.size() == size_t(320) * 180 * 4 &&
                    image.pixels[bottom + 1] < 100 && image.pixels[bottom + 2] > 120,
                "Decoded H.264 frames keep the picture height, without the coded padding rows");
        require(movie.video && movie.hasAudio && movie.frame.width == 320 && movie.duration > .35 &&
                    image.pixels[pixel + 2] > 170 && image.pixels[pixel] < 60 &&
                    rendered.pixels[pixel + 2] < 10,
                "Editing the document and shader library cannot change the in-flight output or preview");
        require(std::any_of(movie.pcm.begin(), movie.pcm.end(),
                            [](float value) { return std::abs(value) > .001f; }),
                "Background MP4 contains the snapshot's DSP-processed audio");
        require(!jobs.cancel(id) && !jobs.status(id).cancellable(),
                "Completed files cannot be retroactively cancelled");
        auto wavId = jobs.start(p, library, sf::ExportKind::Audio, path("mix.wav"));
        require(wait(jobs, wavId).state == sf::ExportState::Completed,
                "Background audio exports use independent media and DSP state");
        sf::MediaAsset wav;
        wav.open(path("mix.wav"));
        require(wav.hasAudio && wav.pcm.size() == 4800 * 2, "WAV duration follows its own snapshot");
        auto rangeId = jobs.start(p, library, sf::ExportKind::Audio, path("range.wav"), "project", {.025, .075});
        require(wait(jobs, rangeId).state == sf::ExportState::Completed &&
                    std::abs(jobs.status(rangeId).duration - .05) < 1e-9,
                "A ranged export renders only the work area");
        sf::MediaAsset ranged;
        ranged.open(path("range.wav"));
        require(ranged.hasAudio && ranged.pcm.size() == 2400 * 2, "The ranged WAV lasts the work area");
        auto aviId = jobs.start(p, library, sf::ExportKind::Video, path("lossless.avi"));
        require(wait(jobs, aviId).state == sf::ExportState::Completed,
                "Uncompressed AVI exports complete in the render queue");
        sf::MediaAsset avi;
        avi.open(path("lossless.avi"));
        require(avi.video && avi.hasAudio && avi.at(0).width == 640 && avi.pcm.size() >= 4800 * 2,
                "Exported RGB/PCM AVI files independently decode as video and stereo audio");
        auto longProject = sf::Project::demo();
        longProject.width = 320;
        longProject.height = 180;
        longProject.duration = 600;
        auto protectedMp4 = path("cancel.mp4");
        protect(protectedMp4);
        auto cancelId = jobs.start(longProject, library, sf::ExportKind::Video, protectedMp4);
        auto queued = jobs.start(p, library, sf::ExportKind::Audio, path("competing.wav"));
        require(jobs.status(queued).state == sf::ExportState::Queued,
                "A second immutable export waits in the queue");
        require(jobs.cancel(queued) && jobs.status(queued).state == sf::ExportState::Cancelled,
                "A queued export cancels immediately without starting a codec");
        bool rejected = false;
        try {
            jobs.start(p, library, sf::ExportKind::Video, protectedMp4);
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, "Active destinations cannot be queued twice");
        require(jobs.cancel(cancelId), "Cancellation is accepted before the commit boundary");
        require(wait(jobs, cancelId).state == sf::ExportState::Cancelled && protectedFile(protectedMp4),
                "Accepted worker cancellation preserves the previous MP4 destination");
        auto protectedWav = path("cancel.wav");
        protect(protectedWav);
        auto cancelWavId = jobs.start(longProject, library, sf::ExportKind::Audio, protectedWav);
        require(jobs.cancel(cancelWavId) && wait(jobs, cancelWavId).state == sf::ExportState::Cancelled &&
                    protectedFile(protectedWav),
                "Accepted WAV cancellation preserves the previous destination");
        // Exercise the last cancellation check after all writes and finalization.
        sf::MediaCache media;
        sf::Renderer renderer;
        renderer.initialize();
        renderer.library = library;
        protect(protectedMp4);
        rejected = false;
        try {
            sf::exportVideo(p, media, renderer, protectedMp4, [](float progress) { return progress < 1; });
        } catch (const sf::ExportCancelled &) {
            rejected = true;
        }
        require(rejected && protectedFile(protectedMp4),
                "MP4 checks cancellation immediately before atomic replacement");
        protect(protectedWav);
        rejected = false;
        try {
            sf::exportWav(p, media, protectedWav, [](float progress) { return progress < 1; });
        } catch (const sf::ExportCancelled &) {
            rejected = true;
        }
        require(rejected && protectedFile(protectedWav),
                "WAV checks cancellation immediately before atomic replacement");
        auto failProject = sf::Project::demo();
        failProject.width = 320;
        failProject.height = 180;
        failProject.duration = .1;
        failProject.findClip(failProject.tracks[0].clips[0].id)->effects[0].code = "Invalid HLSL";
        auto failId = jobs.start(failProject, library, sf::ExportKind::Video, protectedMp4);
        auto failed = wait(jobs, failId);
        require(failed.state == sf::ExportState::Failed && !failed.error.empty() &&
                    protectedFile(protectedMp4),
                "A shader failure is reported without replacing the previous file");
        auto invalidId = jobs.start(p, library, sf::ExportKind::Audio, path("absent/mix.wav"));
        require(wait(jobs, invalidId).state == sf::ExportState::Failed,
                "Output creation failures are reported as failed jobs");
        auto infoBefore = jobs.list();
        rejected = false;
        try {
            jobs.start(p, library, sf::ExportKind::Audio, path("wrong.mp4"));
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected && jobs.list().size() == infoBefore.size(),
                "Invalid format is rejected before registering a job");
        auto imagesPath = path(("frames-" + std::to_string(GetTickCount64())).c_str());
        auto images = jobs.start(p, library, sf::ExportKind::Images, imagesPath);
        auto follow = jobs.start(p, library, sf::ExportKind::Audio, path("after-images.wav"));
        require(wait(jobs, images).state == sf::ExportState::Completed &&
                    wait(jobs, follow).state == sf::ExportState::Completed,
                "Queued jobs automatically advance after the previous render");
        sf::MediaAsset png;
        png.open(imagesPath + "/frame-00000000.png");
        require(png.image && png.frame.width == p.width &&
                    std::filesystem::exists(sx::Utf8Path(imagesPath + "/sequence.json")),
                "PNG sequences contain decodable frames and their cadence manifest");
        auto presetId =
            jobs.start(longProject, library, sf::ExportKind::Video, path("preset.mp4"), "portrait1080");
        require(jobs.status(presetId).width == 1080 && jobs.status(presetId).height == 1920 &&
                    longProject.width == 320,
                "Export presets resize the snapshot without changing the project");
        jobs.cancel(presetId);
        wait(jobs, presetId);
        for (int index = 0; index < 4; ++index) {
            auto quick = jobs.start(p, library, sf::ExportKind::Audio,
                                    path(("history-" + std::to_string(index) + ".wav").c_str()));
            require(wait(jobs, quick).state == sf::ExportState::Completed,
                    "Export slot can be reused after a failure or cancellation");
        }
        rejected = false;
        try {
            jobs.status(id);
        } catch (const std::exception &) {
            rejected = true;
        }
        require(jobs.list().size() == 8 && rejected, "Completed job history is bounded and old IDs expire");
        auto shutdownId = jobs.start(longProject, library, sf::ExportKind::Video, protectedMp4);
        jobs.shutdown();
        require(jobs.status(shutdownId).state == sf::ExportState::Cancelled && protectedFile(protectedMp4),
                "Shutdown cancels and joins workers before the multimedia runtime is released");
        bool pending = false;
        for (auto &file : std::filesystem::directory_iterator(directory))
            pending |= file.path().filename().wstring().find(L".pending-") != std::wstring::npos;
        require(!pending, "Cancelled and failed jobs leave no pending output files");
        require(jobs.list().size() <= 8 && jobs.list().front().id == shutdownId,
                "The job catalogue retains at most eight latest exports");
        std::cout << checks << " background export checks passed.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        result = 1;
    }
    MFShutdown();
    CoUninitialize();
    return result;
}
