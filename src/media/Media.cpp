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
    check(MFCreateAttributes(&a, 2), "Media attributes");
    a->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    Com<IMFSourceReader> r;
    check(MFCreateSourceReaderFromURL(sx::Widen(p).c_str(), a.Get(), &r), "Open media");
    return r;
}
static void videoArea(IMFMediaType *type, MediaAsset &asset);
// The first video stream decoded as RGB32, the other streams off.
static void selectVideo(IMFSourceReader *reader, MediaAsset &asset) {
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    Com<IMFMediaType> type;
    MFCreateMediaType(&type);
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    check(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.Get()), "RGB32 decoding");
    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type);
    videoArea(type.Get(), asset);
}
static Com<IWICImagingFactory> wic() {
    Com<IWICImagingFactory> f;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f)),
          "WIC initialization");
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
static void videoArea(IMFMediaType *type, MediaAsset &asset);
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
              "Read image");
        Com<IWICBitmapFrameDecode> b;
        check(d->GetFrame(0, &b), "Image");
        Com<IWICFormatConverter> c;
        check(f->CreateFormatConverter(&c), "Conversion image");
        check(c->Initialize(b.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                            WICBitmapPaletteTypeCustom),
              "Format image");
        c->GetSize(&frame.width, &frame.height);
        if (!frame.width || !frame.height || uint64_t(frame.width) * frame.height > 67108864)
            throw std::runtime_error("Image is too large (64 megapixels maximum)");
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
            check(f->CreateFormatConverter(&precise), "High-precision conversion");
            check(precise->Initialize(b.Get(), GUID_WICPixelFormat128bppRGBAFloat, WICBitmapDitherTypeNone,
                                      nullptr, 0, WICBitmapPaletteTypeCustom),
                  "RGBA float source");
            frame.rgba.resize(size_t(frame.width) * frame.height * 4);
            check(precise->CopyPixels(nullptr, frame.width * 16, UINT(frame.rgba.size() * 4),
                                      reinterpret_cast<BYTE *>(frame.rgba.data())),
                  "High-precision pixels");
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
        selectVideo(reader.Get(), *this);
        if (!frame.width || !frame.height || uint64_t(codedWidth) * codedHeight > 67108864)
            throw std::runtime_error("Unsupported video dimensions");
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
                    throw std::runtime_error("PCM cache creation failed");
            }
            auto store = [&](const float *data, size_t count) {
                if (cacheOutput) {
                    cacheStream.write(reinterpret_cast<const char *>(data), count * 4);
                    cachedSamples += count;
                    if (!cacheStream)
                        throw std::runtime_error("PCM cache write failed");
                } else {
                    if (pcm.size() + count > 128 * 1024 * 1024)
                        throw std::runtime_error("PCM RAM cache exceeds 512 MiB");
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
                      "Stereo 48 kHz audio decoding");
                for (;;) {
                    DWORD flags = 0;
                    Com<IMFSample> sample;
                    LONGLONG time = 0;
                    check(ar->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &time,
                                         &sample),
                          "Read audio");
                    if (sample) {
                        Com<IMFMediaBuffer> b;
                        sample->ConvertToContiguousBuffer(&b);
                        BYTE *data = nullptr;
                        DWORD bytes = 0;
                        check(b->Lock(&data, nullptr, &bytes), "Buffer audio");
                        if (!cacheOutput && pcm.size() + bytes / 4 > 128 * 1024 * 1024) {
                            b->Unlock();
                            throw std::runtime_error(
                                "Audio is too long for the current cache (512 MiB maximum per source)");
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
                            throw std::runtime_error("PCM cache write failed");
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
                    throw std::runtime_error("PCM cache finalization failed");
                cacheOutput->commit();
                diskPcm = mapPcm(cachePath, fingerprint);
                if (!diskPcm)
                    throw std::runtime_error("Invalid PCM cache after writing");
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
        throw std::runtime_error("No decodable video or audio track");
}
// Coded size and display aperture of a decoded video type; the frame takes the aperture.
static void videoArea(IMFMediaType *type, MediaAsset &asset) {
    MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &asset.codedWidth, &asset.codedHeight);
    asset.frame.width = asset.codedWidth;
    asset.frame.height = asset.codedHeight;
    asset.cropX = asset.cropY = 0;
    MFVideoArea area{};
    if (SUCCEEDED(type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, reinterpret_cast<UINT8 *>(&area), sizeof(area),
                                nullptr))) {
        long x = area.OffsetX.value, y = area.OffsetY.value;
        if (x >= 0 && y >= 0 && area.Area.cx > 0 && area.Area.cy > 0 &&
            x + area.Area.cx <= long(asset.codedWidth) && y + area.Area.cy <= long(asset.codedHeight)) {
            asset.cropX = unsigned(x);
            asset.cropY = unsigned(y);
            asset.frame.width = unsigned(area.Area.cx);
            asset.frame.height = unsigned(area.Area.cy);
        }
    }
}
const Image &MediaAsset::at(double seconds) {
    if (image || !video)
        return frame;
    seconds = std::max(0., seconds);
    if (lastTime >= 0 && std::abs(seconds - lastTime) < 1. / 120)
        return frame;
    bool reopened = false;
    // Reading on only moves forward: time going back since the last request (a reversed clip, one frame at a
    // time) needs a seek, while slow motion asking just before the decoded frame keeps reading.
    bool backward = lastRequest >= 0 && seconds < lastRequest - 1e-3 && seconds < lastTime - 1e-3;
    lastRequest = seconds;
    if (lastTime < 0 || backward || seconds < lastTime - 0.04 || seconds > lastTime + 0.25) {
        PROPVARIANT pos;
        PropVariantInit(&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = LONGLONG(seconds * 1e7);
        // Some files refuse to seek: reading on still reaches a later frame; to go back, a new reader
        // starts from the beginning and reads up to the frame.
        HRESULT sought = reader->SetCurrentPosition(GUID_NULL, pos);
        ++seeks;
        if (FAILED(sought) && (lastTime < 0 || seconds < lastTime)) {
            reader = makeReader(path);
            selectVideo(reader.Get(), *this);
            reopened = true;
        }
        if (SUCCEEDED(sought) || reopened)
            lastTime = -1;
    }
    // The decoded frame already reaches the time asked (slow motion asks several times per frame): it stays.
    if (lastTime >= 0 && seconds <= lastTime + 1e-3)
        return frame;
    int limit = reopened ? std::max(1000, int(seconds * 240) + 240) : 1000;
    for (int attempts = 0; attempts < limit; ++attempts) {
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        Com<IMFSample> s;
        check(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &timestamp, &s),
              "Read video");
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            Com<IMFMediaType> type;
            reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type);
            videoArea(type.Get(), *this);
        }
        if (s) {
            Com<IMFMediaBuffer> b;
            check(s->ConvertToContiguousBuffer(&b), "Decoded image");
            Com<IMF2DBuffer> two;
            b.As(&two);
            BYTE *data = nullptr;
            LONG pitch = LONG(std::max(codedWidth, frame.width) * 4);
            DWORD bytes = 0;
            HRESULT lock = two ? two->Lock2D(&data, &pitch) : b->Lock(&data, nullptr, &bytes);
            check(lock, "Video pixels");
            frame.pixels.resize(size_t(frame.width) * frame.height * 4);
            for (unsigned y = 0; y < frame.height; ++y) {
                auto *row = frame.pixels.data() + size_t(y) * frame.width * 4;
                memcpy(row, data + ptrdiff_t(y + cropY) * pitch + ptrdiff_t(cropX) * 4, frame.width * 4);
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
        throw std::runtime_error("Total latency exceeds 10 seconds");
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
            throw std::runtime_error("Mix output not found");
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
            if (c.source.empty() || c.hold || time + frames / 48000. <= c.start ||
                time >= c.start + c.duration + incomingLatency[t.id] / 48000.)
                continue;
            auto &a = get(c.source);
            Buffer clipData(size_t(frames) * 2, 0);
            bool ramped = c.ramped();
            unsigned rampBlock = ~0u;
            double rampFrom = 0, rampTo = 0;
            for (unsigned f = 0; f < frames; ++f) {
                double now = time + f / 48000.;
                if (!c.live(now))
                    continue;
                int mode = int(std::lround(
                    c.param("stretch_mode", now, c.param("preserve_pitch", now, 0) > .5f ? 1 : 0)));
                // Reversed or ramped, each sample is read at its own source time; no grain stretching.
                if (mode > 0 && std::abs(c.speed - 1) > 1e-7 && !c.reverse && !ramped) {
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
                double at;
                if (ramped) {
                    // A ramp is integrated every 64 samples, the source time interpolated in between.
                    unsigned block = f / 64;
                    if (block != rampBlock) {
                        rampBlock = block;
                        double t0 = time + block * 64 / 48000.;
                        rampFrom = c.sourceTime(t0);
                        rampTo = c.sourceTime(t0 + 64 / 48000.);
                    }
                    at = (rampFrom + (rampTo - rampFrom) * ((f % 64) / 64.)) * 48000;
                } else
                    at = c.sourceTime(now) * 48000;
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
                if (!c.live(now))
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
                    if (!clip.live(now))
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
            throw std::runtime_error("Unable to prepare audio output");
        b.prepared = true;
        if (waveOutWrite(device, &b.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            throw std::runtime_error("Audio output failed");
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
    check(b->Lock(&ptr, nullptr, nullptr), "Export write");
    memcpy(ptr, data, bytes);
    b->Unlock();
    b->SetCurrentLength(bytes);
    Com<IMFSample> s;
    MFCreateSample(&s);
    s->AddBuffer(b.Get());
    s->SetSampleTime(time);
    s->SetSampleDuration(duration);
    check(w->WriteSample(stream, s.Get()), "Media encoding");
}
void exportVideo(const Project &p, MediaCache &m, Renderer &r, const std::string &path,
                 const std::function<bool(float)> &progress, ExportRange range) {
    auto extension = sx::Utf8Path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    if (extension == L".avi") {
        exportAvi(p, m, r, path, progress, range);
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
    check(MFCreateSinkWriterFromURL(output.path().c_str(), nullptr, attrs.Get(), &w), "Create MP4");
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
    check(w->AddStream(v.Get(), &video), "H264 track");
    Com<IMFMediaType> vin;
    MFCreateMediaType(&vin);
    v->CopyAllItems(vin.Get());
    vin->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    vin->SetUINT32(MF_MT_DEFAULT_STRIDE, p.width * 4);
    check(w->SetInputMediaType(video, vin.Get(), nullptr), "Video input");
    Com<IMFMediaType> a;
    MFCreateMediaType(&a);
    a->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    a->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    a->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    a->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    a->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    a->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
    DWORD audio;
    check(w->AddStream(a.Get(), &audio), "AAC track");
    Com<IMFMediaType> ain;
    MFCreateMediaType(&ain);
    a->CopyAllItems(ain.Get());
    ain->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    ain->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    ain->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 192000);
    check(w->SetInputMediaType(audio, ain.Get(), nullptr), "Audio input");
    check(w->BeginWriting(), "Begin export");
    const double from = range.from(p);
    const int total = std::max(1, int(std::ceil((range.to(p) - from) * p.fps - 1e-6)));
    uint64_t previous = 0;
    try {
        for (int f = 0; f < total; ++f) {
            if (progress && !progress(float(f) / total))
                throw ExportCancelled();
            double t = from + double(f) / p.fps;
            r.render(p, m, t, p.width, p.height);
            auto frame = r.readback();
            LONGLONG begin = LONGLONG(f) * 10000000 / p.fps, end = LONGLONG(f + 1) * 10000000 / p.fps;
            putSample(w.Get(), video, frame.pixels.data(), DWORD(frame.pixels.size()), begin, end - begin);
            uint64_t next = uint64_t(f + 1) * 48000 / p.fps;
            std::vector<float> mix;
            m.mix(p, from + previous / 48000., unsigned(next - previous), mix);
            std::vector<short> pcm(mix.size());
            for (size_t i = 0; i < mix.size(); ++i)
                pcm[i] = short(mix[i] * 32767);
            putSample(w.Get(), audio, reinterpret_cast<BYTE *>(pcm.data()), DWORD(pcm.size() * 2),
                      previous * 10000000 / 48000, (next - previous) * 10000000 / 48000);
            previous = next;
        }
        check(w->Finalize(), "MP4 finalization");
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
               const std::function<bool(float)> &progress, ExportRange range) {
    m.resetAudio();
    p.validate();
    const double from = range.from(p);
    uint64_t frames = uint64_t((range.to(p) - from) * 48000 + .5);
    if (frames * 4 > 0xffffffff - 36)
        throw std::runtime_error("WAV exceeds the 32-bit RIFF size limit");
    if (progress && !progress(0))
        throw ExportCancelled();
    AtomicFile output(path);
    std::ofstream file(output.path(), std::ios::binary);
    if (!file)
        throw std::runtime_error("WAV creation failed");
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
        m.mix(p, from + at / 48000., unsigned(std::min<uint64_t>(4096, frames - at)), mix);
        std::vector<short> pcm(mix.size());
        for (size_t i = 0; i < mix.size(); ++i)
            pcm[i] = short(mix[i] * 32767);
        file.write(reinterpret_cast<char *>(pcm.data()), std::streamsize(pcm.size() * 2));
    }
    file.flush();
    if (!file)
        throw std::runtime_error("WAV write failed");
    file.close();
    if (!file)
        throw std::runtime_error("WAV close failed");
    if (progress && !progress(1))
        throw ExportCancelled();
    output.commit();
}
void exportImages(const Project &p, MediaCache &media, Renderer &renderer, const std::string &directory,
                  const std::function<bool(float)> &progress, ExportRange range) {
    p.validate();
    auto destination = std::filesystem::absolute(sx::Utf8Path(directory)).lexically_normal();
    if (std::filesystem::exists(destination))
        throw std::runtime_error("Sequence folder already exists");
    auto staging = destination;
    staging +=
        L".pending-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(staging))
        throw std::runtime_error("PNG folder creation failed");
    // Only files created in this unique staging directory are removed on failure.
    std::vector<std::filesystem::path> created;
    try {
        const double from = range.from(p);
        int frames = std::max(1, int(std::ceil((range.to(p) - from) * p.fps - 1e-6)));
        for (int frame = 0; frame < frames; ++frame) {
            if (progress && !progress(float(frame) / frames))
                throw ExportCancelled();
            renderer.render(p, media, from + double(frame) / p.fps, p.width, p.height);
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
        manifest["start"] = from;
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
            throw std::runtime_error("PNG index write failed");
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
    check(s->InitializeFromFilename(output.path().c_str(), GENERIC_WRITE), "PNG file");
    Com<IWICBitmapEncoder> e;
    f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &e);
    check(e->Initialize(s.Get(), WICBitmapEncoderNoCache), "PNG encoder");
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
    check(e->Commit(), "PNG finalization");
    frame.Reset();
    e.Reset();
    s.Reset();
    output.commit();
}
void exportGif(const Project &p, MediaCache &media, Renderer &renderer, const std::string &path, ExportRange range,
               unsigned width, int fps, const std::function<bool(float)> &progress) {
    width = std::clamp(width, 16u, 1920u) / 2 * 2;
    unsigned height = std::max(2u, unsigned(std::lround(double(width) * p.height / p.width)) / 2 * 2);
    fps = std::clamp(fps, 1, 50);
    double from = range.from(p), to = range.to(p);
    int frames = std::max(1, int(std::floor((to - from) * fps + 1e-6)));
    if (frames > 3000)
        throw std::runtime_error("GIF is too long: 3000 frames maximum");
    AtomicFile output(path);
    auto factory = wic();
    Com<IWICStream> stream;
    factory->CreateStream(&stream);
    check(stream->InitializeFromFilename(output.path().c_str(), GENERIC_WRITE), "GIF file");
    Com<IWICBitmapEncoder> encoder;
    check(factory->CreateEncoder(GUID_ContainerFormatGif, nullptr, &encoder), "GIF encoder");
    check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "GIF encoder");
    {
        // NETSCAPE2.0 application block: loop forever.
        Com<IWICMetadataQueryWriter> global;
        if (SUCCEEDED(encoder->GetMetadataQueryWriter(&global))) {
            static unsigned char application[] = {'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0'};
            static unsigned char loop[] = {3, 1, 0, 0, 0};
            PROPVARIANT value;
            PropVariantInit(&value);
            value.vt = VT_UI1 | VT_VECTOR;
            value.caub = {sizeof(application), application};
            global->SetMetadataByName(L"/appext/application", &value);
            value.caub = {sizeof(loop), loop};
            global->SetMetadataByName(L"/appext/data", &value);
        }
    }
    for (int index = 0; index < frames; ++index) {
        if (progress && !progress(float(index) / frames))
            throw ExportCancelled();
        renderer.render(p, media, from + double(index) / fps, width, height);
        auto image = renderer.readback();
        Com<IWICBitmap> bitmap;
        check(factory->CreateBitmapFromMemory(image.width, image.height, GUID_WICPixelFormat32bppBGRA, image.width * 4,
                                              UINT(image.pixels.size()), image.pixels.data(), &bitmap),
              "Image GIF");
        Com<IWICPalette> palette;
        factory->CreatePalette(&palette);
        check(palette->InitializeFromBitmap(bitmap.Get(), 256, FALSE), "Palette GIF");
        Com<IWICFormatConverter> indexed;
        factory->CreateFormatConverter(&indexed);
        check(indexed->Initialize(bitmap.Get(), GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeErrorDiffusion,
                                  palette.Get(), 0, WICBitmapPaletteTypeCustom),
              "GIF colors");
        Com<IWICBitmapFrameEncode> frame;
        Com<IPropertyBag2> props;
        check(encoder->CreateNewFrame(&frame, &props), "Image GIF");
        check(frame->Initialize(props.Get()), "Image GIF");
        frame->SetSize(width, height);
        WICPixelFormatGUID format = GUID_WICPixelFormat8bppIndexed;
        frame->SetPixelFormat(&format);
        frame->SetPalette(palette.Get());
        Com<IWICMetadataQueryWriter> meta;
        if (SUCCEEDED(frame->GetMetadataQueryWriter(&meta))) {
            // Delay in hundredths of a second.
            PROPVARIANT delay;
            PropVariantInit(&delay);
            delay.vt = VT_UI2;
            delay.uiVal = USHORT(std::lround(100. / fps));
            meta->SetMetadataByName(L"/grctlext/Delay", &delay);
        }
        check(frame->WriteSource(indexed.Get(), nullptr), "Pixels GIF");
        check(frame->Commit(), "Image GIF");
    }
    check(encoder->Commit(), "GIF finalization");
    encoder.Reset();
    stream.Reset();
    output.commit();
    if (progress)
        progress(1);
}
std::vector<std::pair<double, double>> detectSilences(const MediaAsset &asset, double begin, double end,
                                                      float thresholdDb, double minLength) {
    constexpr size_t window = 480;
    size_t frames = asset.samples() / 2;
    size_t first = std::min(frames, size_t(std::max(0., begin) * 48000));
    size_t last = std::min(frames, size_t(std::max(0., end) * 48000));
    double threshold = std::pow(10., thresholdDb / 20);
    std::vector<std::pair<double, double>> spans;
    double quietFrom = -1;
    auto close = [&](double at) {
        if (quietFrom >= 0 && at - quietFrom >= minLength - 1e-9)
            spans.push_back({quietFrom, at});
        quietFrom = -1;
    };
    for (size_t at = first; at < last; at += window) {
        size_t to = std::min(last, at + window);
        double sum = 0;
        for (size_t f = at; f < to; ++f) {
            double mono = .5 * (asset.sample(f * 2) + asset.sample(f * 2 + 1));
            sum += mono * mono;
        }
        bool quiet = std::sqrt(sum / double(std::max<size_t>(1, to - at))) < threshold;
        double time = double(at) / 48000;
        if (quiet && quietFrom < 0)
            quietFrom = time;
        else if (!quiet)
            close(time);
    }
    // A medium shorter than the span is silent after its end.
    if (quietFrom < 0 && end > double(last) / 48000)
        quietFrom = double(last) / 48000;
    close(std::max(double(last) / 48000, end));
    return spans;
}
double soundOffset(const MediaAsset &reference, double referenceIn, double referenceLength, const MediaAsset &moved,
                   double movedIn, double movedLength, double maxLag, double &confidence) {
    constexpr size_t window = 480; // 10 ms
    auto envelope = [&](const MediaAsset &asset, double in, double length) {
        std::vector<double> values;
        size_t frames = asset.samples() / 2;
        size_t first = std::min(frames, size_t(std::max(0., in) * 48000));
        size_t last = std::min(frames, first + size_t(std::max(0., length) * 48000));
        for (size_t at = first; at + window <= last; at += window) {
            double sum = 0;
            for (size_t f = at; f < at + window; ++f) {
                double mono = .5 * (asset.sample(f * 2) + asset.sample(f * 2 + 1));
                sum += mono * mono;
            }
            values.push_back(std::sqrt(sum / window));
        }
        // Centred and scaled, so loud and quiet recordings of one scene compare.
        double mean = 0, energy = 0;
        for (double v : values)
            mean += v;
        mean /= double(std::max<size_t>(1, values.size()));
        for (auto &v : values) {
            v -= mean;
            energy += v * v;
        }
        double scale = energy > 1e-12 ? 1 / std::sqrt(energy) : 0;
        for (auto &v : values)
            v *= scale;
        return values;
    };
    auto a = envelope(reference, referenceIn, referenceLength), b = envelope(moved, movedIn, movedLength);
    confidence = 0;
    if (a.size() < 50 || b.size() < 50)
        throw std::runtime_error("Audio is too short to synchronize (at least half a second required)");
    int lagMax = int(maxLag * 100);
    double best = -2;
    int bestLag = 0;
    // b[i] lines up with a[i + lag]: lag > 0 when the moved sound starts later in the scene.
    for (int lag = -lagMax; lag <= lagMax; ++lag) {
        double sum = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            long j = long(i) + lag;
            if (j >= 0 && j < long(a.size()))
                sum += b[i] * a[size_t(j)];
        }
        if (sum > best) {
            best = sum;
            bestLag = lag;
        }
    }
    confidence = std::clamp(best, 0., 1.);
    return bestLag / 100.;
}
std::vector<std::pair<double, double>> silenceSpans(const Clip &c, const MediaAsset &asset, float thresholdDb,
                                                    double minLength, double padding, int fps) {
    std::vector<std::pair<double, double>> spans;
    double clipEnd = c.start + c.duration;
    for (auto [from, to] : detectSilences(asset, c.in, c.in + c.duration * c.speed, thresholdDb, minLength)) {
        double a = std::max(c.start, c.start + (from - c.in) / c.speed + padding);
        double b = std::min(clipEnd, c.start + (to - c.in) / c.speed - padding);
        a = std::round(a * fps) / fps;
        b = std::round(b * fps) / fps;
        if (b - a >= 1. / fps)
            spans.push_back({a, b});
    }
    return spans;
}
std::vector<double> detectScenes(MediaAsset &asset, double begin, double end, double step, float threshold,
                                 const std::function<bool(float)> &progress) {
    std::vector<double> cuts;
    if (!asset.video || asset.image || step <= 0)
        return cuts;
    std::vector<std::array<float, 32>> histograms;
    std::vector<std::array<float, 144>> cells; // mean luminance on a 16 x 9 grid
    std::vector<double> times;
    for (double t = std::max(0., begin); t < end - 1e-9; t += step) {
        if (progress && !progress(float((t - begin) / std::max(1e-9, end - begin))))
            return {};
        auto &image = asset.at(t);
        std::array<float, 32> histogram{};
        std::array<float, 144> grid{}, gridCount{};
        unsigned stride = std::max(1u, image.width / 64), rows = std::max(1u, image.height / 36);
        float count = 0;
        for (unsigned y = 0; y < image.height; y += rows)
            for (unsigned x = 0; x < image.width; x += stride) {
                auto *px = image.pixels.data() + (size_t(y) * image.width + x) * 4;
                float luma = .0722f * px[0] + .7152f * px[1] + .2126f * px[2];
                histogram[std::min(31, int(luma / 8))] += 1;
                size_t cell = size_t(std::min(8u, y * 9 / std::max(1u, image.height))) * 16 +
                              std::min(15u, x * 16 / std::max(1u, image.width));
                grid[cell] += luma;
                gridCount[cell] += 1;
                ++count;
            }
        for (auto &bin : histogram)
            bin /= std::max(1.f, count);
        for (size_t cell = 0; cell < grid.size(); ++cell)
            grid[cell] /= std::max(1.f, gridCount[cell]);
        histograms.push_back(histogram);
        cells.push_back(grid);
        times.push_back(t);
    }
    std::vector<float> distance(histograms.size(), 0);
    for (size_t n = 1; n < histograms.size(); ++n) {
        // Tones (histogram) or layout (grid): a cut between two dark shots changes the second.
        float d = 0, moved = 0;
        for (int b = 0; b < 32; ++b)
            d += std::abs(histograms[n][size_t(b)] - histograms[n - 1][size_t(b)]);
        for (size_t cell = 0; cell < 144; ++cell)
            moved += std::abs(cells[n][cell] - cells[n - 1][cell]);
        distance[n] = std::max(d * .5f, std::min(1.f, moved / 144 / 24));
    }
    for (size_t n = 1; n < distance.size(); ++n) {
        // A cut stands out from the motion around it; a fade spreads over many frames and does not.
        size_t from = n > 6 ? n - 6 : 1, to = std::min(distance.size(), n + 7);
        float around = 0;
        int samples = 0;
        for (size_t k = from; k < to; ++k)
            if (k != n) {
                around += distance[k];
                ++samples;
            }
        around /= float(std::max(1, samples));
        if (distance[n] >= threshold && distance[n] > around * 3 && distance[n] >= distance[n - 1] &&
            (n + 1 >= distance.size() || distance[n] >= distance[n + 1]))
            cuts.push_back(times[n]);
    }
    return cuts;
}
std::vector<double> detectOnsets(const MediaAsset &asset, double begin, double end, float sensitivity,
                                 double minGap) {
    constexpr size_t hop = 512;
    size_t frames = asset.samples() / 2;
    size_t first = std::min(frames, size_t(std::max(0., begin) * 48000));
    size_t last = std::min(frames, size_t(std::max(0., end) * 48000));
    std::vector<double> onsets;
    if (last <= first + hop * 4)
        return onsets;
    // Energy of the first difference (transients stand out from sustained tones), log-compressed per hop.
    std::vector<float> energy;
    float previous = 0;
    for (size_t at = first; at + hop <= last; at += hop) {
        double sum = 0;
        for (size_t f = at; f < at + hop; ++f) {
            float mono = .5f * (asset.sample(f * 2) + asset.sample(f * 2 + 1));
            float rise = mono - previous;
            previous = mono;
            sum += double(rise) * rise;
        }
        energy.push_back(float(std::log1p(1000 * sum / hop)));
    }
    std::vector<float> flux(energy.size(), 0);
    for (size_t n = 1; n < energy.size(); ++n)
        flux[n] = std::max(0.f, energy[n] - energy[n - 1]);
    float scale = 0;
    for (float value : flux)
        scale = std::max(scale, value);
    if (scale <= 1e-6f)
        return onsets;
    float factor = 2.2f - 1.6f * std::clamp(sensitivity, 0.f, 1.f), floor = scale * (.3f - .25f * sensitivity);
    double lastOnset = -1e9;
    for (size_t n = 1; n + 1 < flux.size(); ++n) {
        // Local peak above both an absolute floor and the mean of its neighbourhood.
        size_t from = n > 12 ? n - 12 : 0, to = std::min(flux.size(), n + 13);
        double mean = 0;
        for (size_t k = from; k < to; ++k)
            mean += flux[k];
        mean /= double(to - from);
        if (flux[n] < flux[n - 1] || flux[n] < flux[n + 1] || flux[n] < floor || flux[n] < mean * factor)
            continue;
        double time = double(first + n * hop) / 48000;
        if (time - lastOnset < minGap)
            continue;
        onsets.push_back(time);
        lastOnset = time;
    }
    return onsets;
}
} // namespace sf
