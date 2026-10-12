#include "media/Media.h"
#include "core/AtomicFile.h"
#include "render/Renderer.h"
#include <fstream>
#include <iomanip>
#include <mfapi.h>
#include <mferror.h>
#include <propvarutil.h>
#include <sstream>
#include <wincodec.h>

namespace sf {
void check(HRESULT hr, const char *what) {
    if (FAILED(hr)) {
        std::ostringstream s;
        s << what << " (0x" << std::hex << unsigned(hr) << ")";
        throw std::runtime_error(s.str());
    }
}
static Com<IMFSourceReader> makeReader(const std::string &p) {
    Com<IMFAttributes> a;
    check(MFCreateAttributes(&a, 2), "Attributs média");
    a->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    Com<IMFSourceReader> r;
    check(MFCreateSourceReaderFromURL(sx::Widen(p).c_str(), a.Get(), &r), "Ouverture média");
    return r;
}
static Com<IWICImagingFactory> wic() {
    Com<IWICImagingFactory> f;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f)),
          "Initialisation WIC");
    return f;
}
PcmMapping::~PcmMapping() {
    if (view)
        UnmapViewOfFile(view);
    if (mapping)
        CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
}
static uint64_t mediaFingerprint(const std::string &source) {
    auto path = std::filesystem::weakly_canonical(sx::Utf8Path(source));
    auto text = sx::Narrow(path.wstring()) + "|" + std::to_string(std::filesystem::file_size(path)) + "|" +
                std::to_string(std::filesystem::last_write_time(path).time_since_epoch().count());
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}
static std::unique_ptr<PcmMapping> mapPcm(const std::filesystem::path &path, uint64_t fingerprint) {
    auto result = std::make_unique<PcmMapping>();
    result->file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
    if (result->file == INVALID_HANDLE_VALUE)
        return {};
    LARGE_INTEGER bytes{};
    if (!GetFileSizeEx(result->file, &bytes) || bytes.QuadPart < 24)
        return {};
    result->mapping = CreateFileMappingW(result->file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!result->mapping)
        return {};
    result->view = MapViewOfFile(result->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!result->view)
        return {};
    auto header = static_cast<const uint64_t *>(result->view);
    if (header[0] != 0x314d43504653ull || header[1] != fingerprint || header[2] % 2 ||
        header[2] > (uint64_t(bytes.QuadPart) - 24) / 4 || header[2] * 4 + 24 != uint64_t(bytes.QuadPart))
        return {};
    result->count = size_t(header[2]);
    result->data = reinterpret_cast<const float *>(header + 3);
    return result;
}
void MediaAsset::open(const std::string &source, const std::filesystem::path &cacheDirectory,
                      bool decodeAudio) {
    path = source;
    auto ext = sx::Utf8Path(path).extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    image = ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".tif" ||
            ext == L".tiff" || ext == L".exr";
    if (image) {
        if (ext == L".exr") {
            frame = readExr(path);
            video = true;
            duration = 6;
            return;
        }
        auto f = wic();
        Com<IWICBitmapDecoder> d;
        check(f->CreateDecoderFromFilename(sx::Widen(path).c_str(), nullptr, GENERIC_READ,
                                           WICDecodeMetadataCacheOnLoad, &d),
              "Lecture image");
        Com<IWICBitmapFrameDecode> b;
        check(d->GetFrame(0, &b), "Image");
        Com<IWICFormatConverter> c;
        check(f->CreateFormatConverter(&c), "Conversion image");
        check(c->Initialize(b.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                            WICBitmapPaletteTypeCustom),
              "Format image");
        c->GetSize(&frame.width, &frame.height);
        if (!frame.width || !frame.height || uint64_t(frame.width) * frame.height > 67108864)
            throw std::runtime_error("Image trop grande (64 mégapixels maximum)");
        frame.pixels.resize(size_t(frame.width) * frame.height * 4);
        check(c->CopyPixels(nullptr, frame.width * 4, UINT(frame.pixels.size()), frame.pixels.data()),
              "Pixels image");
        WICPixelFormatGUID sourceFormat{};
        b->GetPixelFormat(&sourceFormat);
        Com<IWICComponentInfo> info;
        Com<IWICPixelFormatInfo> pixelInfo;
        UINT bits = 0;
        if (SUCCEEDED(f->CreateComponentInfo(sourceFormat, &info)) && SUCCEEDED(info.As(&pixelInfo)) &&
            SUCCEEDED(pixelInfo->GetBitsPerPixel(&bits)) && bits > 32) {
            Com<IWICFormatConverter> precise;
            check(f->CreateFormatConverter(&precise), "Conversion précise");
            check(precise->Initialize(b.Get(), GUID_WICPixelFormat128bppRGBAFloat, WICBitmapDitherTypeNone,
                                      nullptr, 0, WICBitmapPaletteTypeCustom),
                  "RGBA float source");
            frame.rgba.resize(size_t(frame.width) * frame.height * 4);
            check(precise->CopyPixels(nullptr, frame.width * 16, UINT(frame.rgba.size() * 4),
                                      reinterpret_cast<BYTE *>(frame.rgba.data())),
                  "Pixels haute précision");
        }
        video = true;
        duration = 6;
        return;
    }
    reader = makeReader(path);
    PROPVARIANT dur;
    PropVariantInit(&dur);
    if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &dur)) &&
        dur.vt == VT_UI8)
        duration = double(dur.uhVal.QuadPart) / 1e7;
    PropVariantClear(&dur);
    Com<IMFMediaType> native;
    video = SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &native));
    hasAudio = SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &native));
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (video) {
        Com<IMFMediaType> type;
        MFCreateMediaType(&type);
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        check(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.Get()),
              "Décodage RGB32");
        reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type);
        MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &frame.width, &frame.height);
        if (!frame.width || !frame.height || uint64_t(frame.width) * frame.height > 67108864)
            throw std::runtime_error("Dimensions vidéo non prises en charge");
    }
    if (hasAudio && decodeAudio) {
        std::filesystem::path cachePath;
        uint64_t fingerprint = 0;
        if (!cacheDirectory.empty()) {
            std::filesystem::create_directories(cacheDirectory);
            fingerprint = mediaFingerprint(path);
            cachePath = cacheDirectory / (std::to_string(fingerprint) + ".sfpcm");
            diskPcm = mapPcm(cachePath, fingerprint);
        }
        if (!diskPcm) {
            std::unique_ptr<AtomicFile> cacheOutput;
            std::ofstream cacheStream;
            uint64_t cachedSamples = 0;
            if (!cachePath.empty()) {
                cacheOutput = std::make_unique<AtomicFile>(sx::Narrow(cachePath.wstring()));
                cacheStream.open(cacheOutput->path(), std::ios::binary);
                uint64_t header[] = {0x314d43504653ull, fingerprint, 0};
                cacheStream.write(reinterpret_cast<const char *>(header), sizeof(header));
                if (!cacheStream)
                    throw std::runtime_error("Création du cache PCM échouée");
            }
            auto store = [&](const float *data, size_t count) {
                if (cacheOutput) {
                    cacheStream.write(reinterpret_cast<const char *>(data), count * 4);
                    cachedSamples += count;
                    if (!cacheStream)
                        throw std::runtime_error("Écriture du cache PCM échouée");
                } else {
                    if (pcm.size() + count > 128 * 1024 * 1024)
                        throw std::runtime_error("Cache RAM PCM supérieur à 512 Mio");
                    pcm.insert(pcm.end(), data, data + count);
                }
            };
            bool nativeAvi = ext == L".avi" && readFrameAviAudio(path, store);
            if (!nativeAvi) {
                auto ar = makeReader(path);
                ar->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
                ar->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
                Com<IMFMediaType> type;
                MFCreateMediaType(&type);
                type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
                type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
                type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
                type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
                type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8);
                type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 384000);
                check(ar->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get()),
                      "Décodage audio stéréo 48 kHz");
                for (;;) {
                    DWORD flags = 0;
                    Com<IMFSample> sample;
                    LONGLONG time = 0;
                    check(ar->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &time,
                                         &sample),
                          "Lecture audio");
                    if (sample) {
                        Com<IMFMediaBuffer> b;
                        sample->ConvertToContiguousBuffer(&b);
                        BYTE *data = nullptr;
                        DWORD bytes = 0;
                        check(b->Lock(&data, nullptr, &bytes), "Buffer audio");
                        if (!cacheOutput && pcm.size() + bytes / 4 > 128 * 1024 * 1024) {
                            b->Unlock();
                            throw std::runtime_error(
                                "Audio trop long pour le cache actuel (512 Mio maximum par média)");
                        }
                        if (cacheOutput) {
                            cacheStream.write(reinterpret_cast<const char *>(data), bytes);
                            cachedSamples += bytes / 4;
                        } else {
                            size_t old = pcm.size();
                            pcm.resize(old + bytes / 4);
                            memcpy(pcm.data() + old, data, bytes);
                        }
                        b->Unlock();
                        if (cacheOutput && !cacheStream)
                            throw std::runtime_error("Écriture du cache PCM échouée");
                    }
                    if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                        break;
                }
            }
            if (cacheOutput) {
                cacheStream.seekp(16);
                cacheStream.write(reinterpret_cast<const char *>(&cachedSamples), 8);
                cacheStream.close();
                if (!cacheStream)
                    throw std::runtime_error("Finalisation du cache PCM échouée");
                cacheOutput->commit();
                diskPcm = mapPcm(cachePath, fingerprint);
                if (!diskPcm)
                    throw std::runtime_error("Cache PCM invalide après écriture");
            }
        }
        peaks.resize((samples() / 2 + 479) / 480);
        for (size_t i = 0; i < samples() / 2; ++i)
            peaks[i / 480] =
                std::max(peaks[i / 480], std::max(std::abs(sample(i * 2)), std::abs(sample(i * 2 + 1))));
        for (auto peak : peaks)
            peakCeiling = std::max(peakCeiling, peak);
        if (duration <= 0)
            duration = samples() / 96000.;
    }
    if (!video && !hasAudio)
        throw std::runtime_error("Aucune piste vidéo ou audio décodable");
}
const Image &MediaAsset::at(double seconds) {
    if (image || !video)
        return frame;
    seconds = std::max(0., seconds);
    if (lastTime >= 0 && std::abs(seconds - lastTime) < 1. / 120)
        return frame;
    if (lastTime < 0 || seconds < lastTime - 0.04 || seconds > lastTime + 0.25) {
        PROPVARIANT pos;
        PropVariantInit(&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = LONGLONG(seconds * 1e7);
        check(reader->SetCurrentPosition(GUID_NULL, pos), "Recherche vidéo");
        lastTime = -1;
    }
    for (int attempts = 0; attempts < 1000; ++attempts) {
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        Com<IMFSample> s;
        check(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &timestamp, &s),
              "Lecture vidéo");
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            Com<IMFMediaType> type;
            reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type);
            MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &frame.width, &frame.height);
        }
        if (s) {
            Com<IMFMediaBuffer> b;
            check(s->ConvertToContiguousBuffer(&b), "Image décodée");
            Com<IMF2DBuffer> two;
            b.As(&two);
            BYTE *data = nullptr;
            LONG pitch = LONG(frame.width * 4);
            DWORD bytes = 0;
            HRESULT lock = two ? two->Lock2D(&data, &pitch) : b->Lock(&data, nullptr, &bytes);
            check(lock, "Pixels vidéo");
            frame.pixels.resize(size_t(frame.width) * frame.height * 4);
            for (unsigned y = 0; y < frame.height; ++y) {
                auto *row = frame.pixels.data() + size_t(y) * frame.width * 4;
                memcpy(row, data + ptrdiff_t(y) * pitch, frame.width * 4);
                for (unsigned x = 0; x < frame.width; ++x)
                    row[x * 4 + 3] = 255;
            }
            if (two)
                two->Unlock2D();
            else
                b->Unlock();
            lastTime = timestamp / 1e7;
            if (lastTime + 0.001 >= seconds)
                break;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
    }
    return frame;
}
MediaAsset &MediaCache::getBlocking(const std::string &p) {
    auto i = assets.find(p);
    if (i != assets.end()) {
        if (!i->second->error.empty())
            throw std::runtime_error(i->second->error);
        return *i->second;
    }
    auto a = std::make_unique<MediaAsset>();
    try {
        a->open(p, diskCacheDirectory);
    } catch (const std::exception &e) {
        a->error = e.what();
        assets[p] = std::move(a);
        throw;
    }
    auto *ptr = a.get();
    assets[p] = std::move(a);
    return *ptr;
}
void MediaCache::Delay::apply(std::vector<float> &data, unsigned frames) {
    if (!frames)
        return;
    if (samples.size() != size_t(frames) * 2) {
        samples.assign(size_t(frames) * 2, 0);
        cursor = 0;
    }
    for (auto &value : data) {
        std::swap(value, samples[cursor]);
        cursor = (cursor + 1) % samples.size();
    }
}
void MediaCache::mix(const Project &p, double time, unsigned frames, std::vector<float> &out) {
    if (lastRequestedEnd < 0 || std::abs(lastRequestedEnd - time) > 1. / 48000) {
        resetAudio();
        std::vector<float> discarded;
        mixEngine(p, time, 0, discarded);
        for (unsigned at = 0; at < latencySamples; at += 4096)
            mixEngine(p, time + at / 48000., std::min(4096u, latencySamples - at), discarded);
        loudness.reset();
    }
    mixEngine(p, time + latencySamples / 48000., frames, out);
    lastRequestedEnd = time + frames / 48000.;
}
void MediaCache::mixEngine(const Project &p, double time, unsigned frames, std::vector<float> &out) {
    if (lastMixEnd < 0 || std::abs(time - lastMixEnd) > 1. / 48000)
        loudness.reset();
    auto order = p.audioBusOrder();
    effects.begin(time);
    std::map<Id, unsigned> incomingLatency, clipLatency, channelLatency;
    auto registerOutput = [&](Id id, const MixChannel &mix, unsigned latency) {
        channelLatency[id] = latency;
        incomingLatency[mix.output] = std::max(incomingLatency[mix.output], latency);
        for (auto &send : mix.sends)
            if (send.enabled)
                incomingLatency[send.bus] = std::max(incomingLatency[send.bus], latency);
    };
    for (auto &track : p.tracks)
        if (track.kind == TrackKind::Audio) {
            unsigned longest = 0;
            for (auto &clip : track.clips) {
                clipLatency[clip.id] = effects.latency(clip.id, clip.effects);
                longest = std::max(longest, clipLatency[clip.id]);
            }
            incomingLatency[track.id] = longest;
            registerOutput(track.id, track.mix, longest + effects.latency(track.id, track.mix.effects));
        }
    for (auto id : order) {
        auto &bus = *p.findBus(id);
        registerOutput(id, bus.mix, incomingLatency[id] + effects.latency(id, bus.mix.effects));
    }
    latencySamples = incomingLatency[0] + effects.latency(0, p.masterMix.effects);
    if (latencySamples > 480000)
        throw std::runtime_error("Latence totale supérieure à 10 secondes");
    mixPeaks.clear();
    mixRms.clear();
    using Buffer = std::vector<float>;
    std::map<Id, Buffer> inputs;
    inputs[0].assign(size_t(frames) * 2, 0);
    for (auto id : order)
        inputs[id].assign(size_t(frames) * 2, 0);
    auto sum = [&](Id destination, const Buffer &data, Id source, bool sendRoute = false) {
        auto it = inputs.find(destination);
        if (it == inputs.end())
            throw std::runtime_error("Sortie de mixage introuvable");
        Buffer aligned = data;
        delays[{source, destination, sendRoute ? 1 : 0}].apply(aligned, incomingLatency[destination] -
                                                                            channelLatency[source]);
        for (size_t n = 0; n < data.size(); ++n)
            it->second[n] += aligned[n];
    };
    auto fader = [&](const MixChannel &channel, float gain, float pan, Buffer &data, bool applyPan = true,
                     unsigned latency = 0) {
        for (unsigned f = 0; f < frames; ++f) {
            double now = time + f / 48000. - latency / 48000.;
            float g = std::pow(10.f, channel.param("gain", now, gain) / 20);
            float balance = applyPan ? std::clamp(channel.param("pan", now, pan), -1.f, 1.f) : 0;
            data[f * 2] *= g * (balance > 0 ? 1 - balance : 1);
            data[f * 2 + 1] *= g * (balance < 0 ? 1 + balance : 1);
        }
    };
    auto meter = [&](Id id, const Buffer &data) {
        auto &peak = mixPeaks[id];
        std::array<double, 2> squares{};
        for (unsigned f = 0; f < frames; ++f)
            for (unsigned channel = 0; channel < 2; ++channel)
                if (std::isfinite(data[f * 2 + channel])) {
                    peak[channel] = std::max(peak[channel], std::abs(data[f * 2 + channel]));
                    squares[channel] += double(data[f * 2 + channel]) * data[f * 2 + channel];
                }
        for (unsigned channel = 0; channel < 2; ++channel)
            mixRms[id][channel] = float(std::sqrt(squares[channel] / std::max(1u, frames)));
    };
    auto route = [&](Id id, const MixChannel &channel, float gain, float pan, Buffer &data, bool muted,
                     bool applyPan = true) {
        effects.process(id, channel.effects, time, frames, data);
        if (muted) {
            mixPeaks[id] = {};
            return;
        }
        auto sendBuffer = [&](const AudioSend &send) {
            Buffer sent = data;
            for (unsigned frame = 0; frame < frames; ++frame) {
                float gain = std::pow(10.f, channel.param("send." + std::to_string(send.bus),
                                                          time + frame / 48000. - channelLatency[id] / 48000.,
                                                          send.gain) /
                                                20);
                sent[frame * 2] *= gain;
                sent[frame * 2 + 1] *= gain;
            }
            sum(send.bus, sent, id, true);
        };
        for (auto &send : channel.sends)
            if (send.enabled && send.preFader)
                sendBuffer(send);
        fader(channel, gain, pan, data, applyPan, channelLatency[id]);
        meter(id, data);
        sum(channel.output, data, id);
        for (auto &send : channel.sends)
            if (send.enabled && !send.preFader)
                sendBuffer(send);
    };
    for (auto &t : p.tracks) {
        if (t.kind != TrackKind::Audio)
            continue;
        if (!p.audible(t)) {
            mixPeaks[t.id] = {};
            continue;
        }
        Buffer trackData(size_t(frames) * 2, 0);
        for (auto &c : t.clips) {
            if (c.source.empty() || time + frames / 48000. <= c.start ||
                time >= c.start + c.duration + incomingLatency[t.id] / 48000.)
                continue;
            auto &a = get(c.source);
            Buffer clipData(size_t(frames) * 2, 0);
            for (unsigned f = 0; f < frames; ++f) {
                double now = time + f / 48000.;
                if (!c.active(now))
                    continue;
                int mode = int(std::lround(
                    c.param("stretch_mode", now, c.param("preserve_pitch", now, 0) > .5f ? 1 : 0)));
                if (mode > 0 && std::abs(c.speed - 1) > 1e-7) {
                    // WSOLA: align overlapping unit-speed grains by waveform correlation.
                    // Grain sizes follow CyDAW v2 tonal/beats/texture modes (80/28/180 ms).
                    double sourceBegin = c.in * 48000,
                           sourceEnd =
                               std::min(double(a.samples() / 2), (c.in + c.duration * c.speed) * 48000);
                    if (sourceEnd - sourceBegin < 2)
                        continue;
                    int length = std::min(mode == 2   ? 1344
                                          : mode == 3 ? 8640
                                                      : 3840,
                                          std::max(2, int((sourceEnd - sourceBegin - 1) / 4) * 2)),
                        hop = length / 2;
                    double output = (now - c.start) * 48000;
                    int64_t grain = int64_t(std::floor(output / hop));
                    auto &stretch = stretches[c.id];
                    auto read = [&](double source, unsigned channel) {
                        if (source < c.in * 48000 ||
                            source + 1 >=
                                std::min(double(a.samples() / 2), (c.in + c.duration * c.speed) * 48000))
                            return 0.f;
                        size_t index = size_t(source);
                        float u = float(source - index);
                        return a.sample(index * 2 + channel) * (1 - u) +
                               a.sample(index * 2 + 2 + channel) * u;
                    };
                    if (stretch.speed != c.speed || stretch.in != c.in || stretch.mode != mode ||
                        grain < stretch.grain || grain > stretch.grain + 1) {
                        stretch = {};
                        stretch.speed = c.speed;
                        stretch.in = c.in;
                        stretch.mode = mode;
                    }
                    if (stretch.grain != grain) {
                        // Keep the complete unit-speed grain within trimmed source handles.
                        double expected = std::clamp(c.in * 48000 + grain * hop * c.speed, sourceBegin,
                                                     std::max(sourceBegin, sourceEnd - length - 1));
                        if (stretch.grain < 0) {
                            stretch.previous = expected - hop;
                            stretch.current = expected;
                        } else {
                            stretch.previous = stretch.current;
                            double best = 1e30;
                            double chosen = expected;
                            for (int offset = mode == 2 ? 0 : -256; offset <= (mode == 2 ? 0 : 256);
                                 ++offset) {
                                double candidate = expected + offset;
                                if (candidate < sourceBegin || candidate + length >= sourceEnd)
                                    continue;
                                double score = 0;
                                for (int sample = 0; sample < 256; sample += 4) {
                                    double left = read(stretch.previous + hop + sample, 0) +
                                                  read(stretch.previous + hop + sample, 1);
                                    double right = read(candidate + sample, 0) + read(candidate + sample, 1);
                                    double difference = left - right;
                                    score += difference * difference;
                                }
                                score += std::abs(offset) * 1e-12;
                                if (score < best) {
                                    best = score;
                                    chosen = candidate;
                                }
                            }
                            stretch.current = chosen;
                        }
                        stretch.grain = grain;
                    }
                    double weightSum = 0;
                    for (int offset = 0; offset < 2; ++offset) {
                        int64_t index = grain - offset;
                        double phase = output - index * hop;
                        if (grain == 0 && offset == 1)
                            continue;
                        double weight = std::pow(std::sin(3.141592653589793 * (phase + .5) / length), 2);
                        double source = (offset ? stretch.previous : stretch.current) + phase;
                        if (source < c.in * 48000 ||
                            source + 1 >=
                                std::min(double(a.samples() / 2), (c.in + c.duration * c.speed) * 48000))
                            continue;
                        size_t at = size_t(source);
                        float fraction = float(source - at);
                        weightSum += weight;
                        for (unsigned channel = 0; channel < 2; ++channel)
                            clipData[f * 2 + channel] +=
                                float(weight) * (a.sample(at * 2 + channel) * (1 - fraction) +
                                                 a.sample(at * 2 + 2 + channel) * fraction);
                    }
                    if (weightSum > 1e-9)
                        for (unsigned channel = 0; channel < 2; ++channel)
                            clipData[f * 2 + channel] /= float(weightSum);
                    continue;
                }
                double at = c.sourceTime(now) * 48000;
                auto index = size_t(at);
                if (index * 2 + 3 >= a.samples())
                    continue;
                float u = float(at - index);
                clipData[f * 2] = a.sample(index * 2) * (1 - u) + a.sample(index * 2 + 2) * u;
                clipData[f * 2 + 1] = a.sample(index * 2 + 1) * (1 - u) + a.sample(index * 2 + 3) * u;
            }
            effects.process(c, time, frames, clipData);
            delays[{c.id, t.id, 2}].apply(clipData, incomingLatency[t.id] - clipLatency[c.id]);
            for (unsigned f = 0; f < frames; ++f) {
                double now = time + f / 48000. - incomingLatency[t.id] / 48000.;
                if (!c.active(now))
                    continue;
                float gain = std::pow(10.f, c.param("gain", now, 0) / 20) * c.envelope(now);
                float pan = std::clamp(t.mix.param("pan", now, t.pan) + c.param("pan", now, 0), -1.f, 1.f);
                trackData[f * 2] += clipData[f * 2] * gain * (pan > 0 ? 1 - pan : 1);
                trackData[f * 2 + 1] += clipData[f * 2 + 1] * gain * (pan < 0 ? 1 + pan : 1);
            }
        }
        route(t.id, t.mix, t.gain, t.pan, trackData, false, false);
    }
    for (auto id : order) {
        auto &bus = *p.findBus(id);
        route(id, bus.mix, bus.gain, bus.pan, inputs.at(id), bus.muted);
    }
    // Video containers carry their local audio timeline into the parent master.
    for (auto &track : p.tracks)
        if (track.kind == TrackKind::Video && p.audible(track))
            for (auto &clip : track.clips) {
                if (clip.composition.IsNull() || !frames || time + frames / 48000. <= clip.start ||
                    time >= clip.start + clip.duration)
                    continue;
                auto &entry = nested[clip.id];
                auto identity = clip.composition.Dump(0);
                if (entry.identity != identity) {
                    entry.project = Project::fromJson(clip.composition);
                    entry.media = std::make_unique<MediaCache>();
                    entry.identity = identity;
                    entry.audio.clear();
                    entry.audioStart = -1;
                }
                unsigned first = unsigned(std::max(0., std::ceil((clip.start - time) * 48000.)));
                first = std::min(first, frames);
                if (first == frames)
                    continue;
                auto childTime = clip.sourceTime(time + first / 48000.);
                auto &child = entry.audio;
                double phase = (childTime - entry.audioStart) * 48000.;
                if (child.empty() || phase < -1e-4 || phase > child.size() / 2.) {
                    child.clear();
                    entry.audioStart = childTime;
                    phase = 0;
                } else {
                    auto consumed = std::min(child.size() / 2, size_t(std::floor(std::max(0., phase))));
                    child.erase(child.begin(), child.begin() + consumed * 2);
                    entry.audioStart += consumed / 48000.;
                    phase = std::max(0., phase - consumed);
                }
                // Keep interpolation lookahead for the next block; replaying it would reset child DSPs.
                unsigned count = unsigned(std::floor(phase + (frames - first - 1) * clip.speed)) + 2;
                std::vector<float> part;
                while (child.size() / 2 < count) {
                    auto offset = unsigned(child.size() / 2);
                    auto n = std::min(2048u, count - offset);
                    entry.media->mix(entry.project, entry.audioStart + offset / 48000., n, part);
                    child.insert(child.end(), part.begin(), part.end());
                }
                for (unsigned f = first; f < frames; ++f) {
                    double now = time + f / 48000.;
                    if (!clip.active(now))
                        continue;
                    double pos = phase + (f - first) * clip.speed;
                    auto at = size_t(pos);
                    float u = float(pos - at),
                          gain = clip.envelope(now) * std::pow(10.f, clip.param("gain", now, 0) / 20.f);
                    float pan = std::clamp(clip.param("pan", now, 0), -1.f, 1.f);
                    for (int ch = 0; ch < 2; ++ch)
                        inputs.at(0)[f * 2 + ch] +=
                            (child[at * 2 + ch] * (1 - u) + child[(at + 1) * 2 + ch] * u) * gain *
                            (ch == 0 && pan > 0   ? 1 - pan
                             : ch == 1 && pan < 0 ? 1 + pan
                                                  : 1);
                }
            }
    std::erase_if(nested, [&](const auto &entry) { return !p.findClip(entry.first); });
    out = std::move(inputs.at(0));
    effects.process(0, p.masterMix.effects, time, frames, out);
    fader(p.masterMix, p.master, 0, out, true, latencySamples);
    meter(0, out);
    for (auto &x : out)
        x = std::isfinite(x) ? std::clamp(x, -1.f, 1.f) : 0.f;
    effects.end(time + frames / 48000.);
    loudness.push(out);
    lastMixEnd = time + frames / 48000.;
}
bool AudioOutput::available() {
    if (device)
        return true;
    WAVEFORMATEX f{};
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = 2;
    f.nSamplesPerSec = 48000;
    f.wBitsPerSample = 16;
    f.nBlockAlign = 4;
    f.nAvgBytesPerSec = 192000;
    return waveOutOpen(&device, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
}
void AudioOutput::start(double t) {
    stop();
    origin = t;
    queued = 0;
    running = available();
}
void AudioOutput::stop() {
    running = false;
    if (device)
        waveOutReset(device);
}
AudioOutput::~AudioOutput() {
    stop();
    if (device) {
        for (auto &b : blocks)
            if (b.prepared)
                waveOutUnprepareHeader(device, &b.header, sizeof(WAVEHDR));
        waveOutClose(device);
    }
}
double AudioOutput::time() const {
    if (!device || !running)
        return origin;
    MMTIME t{};
    t.wType = TIME_SAMPLES;
    if (waveOutGetPosition(device, &t, sizeof(t)) != MMSYSERR_NOERROR)
        return origin;
    return origin + t.u.sample / 48000.;
}
void AudioOutput::pump(const Project &p, MediaCache &m) {
    if (!running)
        return;
    for (auto &b : blocks) {
        if (b.prepared && !(b.header.dwFlags & WHDR_DONE))
            continue;
        std::vector<float> mix;
        m.mix(p, origin + queued / 48000., blockFrames, mix);
        for (size_t i = 0; i < mix.size(); ++i)
            b.samples[i] = short(mix[i] * 32767);
        if (b.prepared) {
            waveOutUnprepareHeader(device, &b.header, sizeof(WAVEHDR));
            b.prepared = false;
        }
        b.header = {};
        b.header.lpData = reinterpret_cast<LPSTR>(b.samples.data());
        b.header.dwBufferLength = sizeof(b.samples);
        if (waveOutPrepareHeader(device, &b.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            throw std::runtime_error("Préparation de la sortie audio échouée");
        b.prepared = true;
        if (waveOutWrite(device, &b.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            throw std::runtime_error("Sortie audio échouée");
        queued += blockFrames;
        analysis.channelCount = 2;
        analysis.waveformCount = blockFrames;
        analysis.peak = {};
        analysis.rms = {};
        for (unsigned i = 0; i < blockFrames; ++i) {
            analysis.waveform[i] = (mix[i * 2] + mix[i * 2 + 1]) * 0.5f;
            for (int ch = 0; ch < 2; ++ch) {
                analysis.peak[ch] = std::max(analysis.peak[ch], std::abs(mix[i * 2 + ch]));
                analysis.rms[ch] += mix[i * 2 + ch] * mix[i * 2 + ch];
            }
        }
        for (int ch = 0; ch < 2; ++ch)
            analysis.rms[ch] = std::sqrt(analysis.rms[ch] / blockFrames);
        ++analysis.sequence;
    }
}
static void putSample(IMFSinkWriter *w, DWORD stream, const BYTE *data, DWORD bytes, LONGLONG time,
                      LONGLONG duration) {
    Com<IMFMediaBuffer> b;
    check(MFCreateMemoryBuffer(bytes, &b), "Buffer export");
    BYTE *ptr;
    check(b->Lock(&ptr, nullptr, nullptr), "Écriture export");
    memcpy(ptr, data, bytes);
    b->Unlock();
    b->SetCurrentLength(bytes);
    Com<IMFSample> s;
    MFCreateSample(&s);
    s->AddBuffer(b.Get());
    s->SetSampleTime(time);
    s->SetSampleDuration(duration);
    check(w->WriteSample(stream, s.Get()), "Encodage média");
}
void exportVideo(const Project &p, MediaCache &m, Renderer &r, const std::string &path,
                 const std::function<bool(float)> &progress) {
    auto extension = sx::Utf8Path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    if (extension == L".avi") {
        exportAvi(p, m, r, path, progress);
        return;
    }
    m.resetAudio();
    p.validate();
    if (progress && !progress(0))
        throw ExportCancelled();
    AtomicFile output(path);
    Com<IMFSinkWriter> w;
    Com<IMFAttributes> attrs;
    MFCreateAttributes(&attrs, 2);
    attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    attrs->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    check(MFCreateSinkWriterFromURL(output.path().c_str(), nullptr, attrs.Get(), &w), "Création MP4");
    Com<IMFMediaType> v;
    MFCreateMediaType(&v);
    v->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    v->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    v->SetUINT32(MF_MT_AVG_BITRATE, 8000000);
    v->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(v.Get(), MF_MT_FRAME_SIZE, p.width, p.height);
    MFSetAttributeRatio(v.Get(), MF_MT_FRAME_RATE, p.fps, 1);
    MFSetAttributeRatio(v.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    DWORD video;
    check(w->AddStream(v.Get(), &video), "Piste H264");
    Com<IMFMediaType> vin;
    MFCreateMediaType(&vin);
    v->CopyAllItems(vin.Get());
    vin->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    vin->SetUINT32(MF_MT_DEFAULT_STRIDE, p.width * 4);
    check(w->SetInputMediaType(video, vin.Get(), nullptr), "Entrée vidéo");
    Com<IMFMediaType> a;
    MFCreateMediaType(&a);
    a->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    a->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    a->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    a->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    a->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    a->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
    DWORD audio;
    check(w->AddStream(a.Get(), &audio), "Piste AAC");
    Com<IMFMediaType> ain;
    MFCreateMediaType(&ain);
    a->CopyAllItems(ain.Get());
    ain->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    ain->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    ain->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 192000);
    check(w->SetInputMediaType(audio, ain.Get(), nullptr), "Entrée audio");
    check(w->BeginWriting(), "Début export");
    const int total = int(std::ceil(p.duration * p.fps));
    uint64_t previous = 0;
    try {
        for (int f = 0; f < total; ++f) {
            if (progress && !progress(float(f) / total))
                throw ExportCancelled();
            double t = double(f) / p.fps;
            r.render(p, m, t, p.width, p.height);
            auto frame = r.readback();
            LONGLONG begin = LONGLONG(f) * 10000000 / p.fps, end = LONGLONG(f + 1) * 10000000 / p.fps;
            putSample(w.Get(), video, frame.pixels.data(), DWORD(frame.pixels.size()), begin, end - begin);
            uint64_t next = uint64_t(f + 1) * 48000 / p.fps;
            std::vector<float> mix;
            m.mix(p, previous / 48000., unsigned(next - previous), mix);
            std::vector<short> pcm(mix.size());
            for (size_t i = 0; i < mix.size(); ++i)
                pcm[i] = short(mix[i] * 32767);
            putSample(w.Get(), audio, reinterpret_cast<BYTE *>(pcm.data()), DWORD(pcm.size() * 2),
                      previous * 10000000 / 48000, (next - previous) * 10000000 / 48000);
            previous = next;
        }
        check(w->Finalize(), "Finalisation MP4");
    } catch (...) {
        w->Finalize();
        throw;
    }
    w.Reset();
    // This callback is the cancellation boundary before atomic replacement.
    if (progress && !progress(1))
        throw ExportCancelled();
    output.commit();
}
void exportWav(const Project &p, MediaCache &m, const std::string &path,
               const std::function<bool(float)> &progress) {
    m.resetAudio();
    p.validate();
    uint64_t frames = uint64_t(p.duration * 48000);
    if (frames * 4 > 0xffffffff - 36)
        throw std::runtime_error("WAV trop long pour RIFF 32 bits");
    if (progress && !progress(0))
        throw ExportCancelled();
    AtomicFile output(path);
    std::ofstream file(output.path(), std::ios::binary);
    if (!file)
        throw std::runtime_error("Création WAV échouée");
    auto u32 = [&](uint32_t n) { file.write(reinterpret_cast<char *>(&n), 4); };
    auto u16 = [&](uint16_t n) { file.write(reinterpret_cast<char *>(&n), 2); };
    file.write("RIFF", 4);
    u32(uint32_t(frames * 4 + 36));
    file.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(2);
    u32(48000);
    u32(192000);
    u16(4);
    u16(16);
    file.write("data", 4);
    u32(uint32_t(frames * 4));
    for (uint64_t at = 0; at < frames; at += 4096) {
        if (progress && !progress(float(double(at) / frames)))
            throw ExportCancelled();
        std::vector<float> mix;
        m.mix(p, at / 48000., unsigned(std::min<uint64_t>(4096, frames - at)), mix);
        std::vector<short> pcm(mix.size());
        for (size_t i = 0; i < mix.size(); ++i)
            pcm[i] = short(mix[i] * 32767);
        file.write(reinterpret_cast<char *>(pcm.data()), std::streamsize(pcm.size() * 2));
    }
    file.flush();
    if (!file)
        throw std::runtime_error("Écriture WAV échouée");
    file.close();
    if (!file)
        throw std::runtime_error("Fermeture WAV échouée");
    if (progress && !progress(1))
        throw ExportCancelled();
    output.commit();
}
void exportImages(const Project &p, MediaCache &media, Renderer &renderer, const std::string &directory,
                  const std::function<bool(float)> &progress) {
    p.validate();
    auto destination = std::filesystem::absolute(sx::Utf8Path(directory)).lexically_normal();
    if (std::filesystem::exists(destination))
        throw std::runtime_error("Le dossier de séquence existe déjà");
    auto staging = destination;
    staging +=
        L".pending-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(staging))
        throw std::runtime_error("Création du dossier PNG échouée");
    // Only files created in this unique staging directory are removed on failure.
    std::vector<std::filesystem::path> created;
    try {
        int frames = int(std::ceil(p.duration * p.fps));
        for (int frame = 0; frame < frames; ++frame) {
            if (progress && !progress(float(frame) / frames))
                throw ExportCancelled();
            renderer.render(p, media, double(frame) / p.fps, p.width, p.height);
            char name[32];
            snprintf(name, sizeof(name), p.imageFormat == "exr16" ? "frame-%08d.exr" : "frame-%08d.png",
                     frame);
            auto file = staging / name;
            created.push_back(file);
            if (p.imageFormat == "exr16")
                writeExr(renderer.readbackFloat(), sx::Narrow(file.wstring()));
            else if (p.imageFormat == "png16")
                writePng16(renderer.readbackFloat(true), sx::Narrow(file.wstring()));
            else
                writePng(renderer.readback(), sx::Narrow(file.wstring()));
        }
        Json manifest = Json::Object();
        manifest["fps"] = p.fps;
        manifest["frames"] = frames;
        manifest["width"] = p.width;
        manifest["height"] = p.height;
        manifest["project"] = p.name;
        manifest["format"] = p.imageFormat;
        manifest["workingSpace"] = p.workingSpace;
        manifest["alpha"] = p.imageFormat == "exr16" ? "premultiplied" : "straight";
        auto index = staging / L"sequence.json";
        created.push_back(index);
        std::ofstream file(index);
        file << manifest.Dump(2);
        file.close();
        if (!file)
            throw std::runtime_error("Écriture de l'index PNG échouée");
        if (progress && !progress(1))
            throw ExportCancelled();
        std::filesystem::rename(staging, destination);
    } catch (...) {
        std::error_code ignored;
        for (auto &file : created)
            std::filesystem::remove(file, ignored);
        std::filesystem::remove(staging, ignored);
        throw;
    }
}
void writePng(const Image &i, const std::string &path) {
    AtomicFile output(path);
    auto f = wic();
    Com<IWICStream> s;
    f->CreateStream(&s);
    check(s->InitializeFromFilename(output.path().c_str(), GENERIC_WRITE), "Fichier PNG");
    Com<IWICBitmapEncoder> e;
    f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &e);
    check(e->Initialize(s.Get(), WICBitmapEncoderNoCache), "Encodeur PNG");
    Com<IWICBitmapFrameEncode> frame;
    Com<IPropertyBag2> props;
    e->CreateNewFrame(&frame, &props);
    frame->Initialize(props.Get());
    frame->SetSize(i.width, i.height);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    check(frame->SetPixelFormat(&fmt), "Format PNG");
    check(
        frame->WritePixels(i.height, i.width * 4, UINT(i.pixels.size()), const_cast<BYTE *>(i.pixels.data())),
        "Pixels PNG");
    check(frame->Commit(), "Image PNG");
    check(e->Commit(), "Finalisation PNG");
    frame.Reset();
    e.Reset();
    s.Reset();
    output.commit();
}
} // namespace sf
