#pragma once

#include "NativeDspGraph.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace cydaw::engine {

// The effects that came after the first primitives, each described once.
//
// A primitive used to be spelled out in a dozen tables — the card type and
// its processor key, the processor key and its native kind, each binding and
// its field, each port and its parameter, the ranges, the labels, the palette
// — and adding one meant finding all of them. Every one of those tables now
// asks this catalogue after its own entries, so an effect is an entry here
// and a processor in EffectProcessors.cpp.
//
// Their values live in the generic effect1..effect8 fields of the payload,
// or, for an effect with many (the parametric equaliser's bands), in slots
// of its eqBands array: where each value lives is the entry's business.

struct EffectParameter {
    // The primitive's parameter and value-port key.
    std::string_view key;
    // The binding suffix a card publishes it under ("chorus.rate").
    std::string_view suffix;
    std::string_view labelKey;
    // "Hz", "ms", "dB" or empty.
    std::string_view unit;
    float minimum {};
    float maximum { 1.0f };
    float defaultValue {};
    // Either a field of the payload...
    float NativeDspNodeParameters::* field {};
    // ...or a slot of its eqBands array. A slot has no value socket: cables
    // drive named fields only.
    std::int16_t slot { -1 };
    // The band it belongs to, 0 based, for its label; -1 for none.
    std::int8_t band { -1 };

    [[nodiscard]] constexpr bool modulatable() const noexcept { return field != nullptr; }
};

[[nodiscard]] inline float effectValue(const NativeDspNodeParameters& parameters, const EffectParameter& parameter) noexcept
{
    if (parameter.field != nullptr)
        return parameters.*(parameter.field);
    if (parameter.slot >= 0 && static_cast<std::size_t>(parameter.slot) < parameters.eqBands.size())
        return parameters.eqBands[static_cast<std::size_t>(parameter.slot)];
    return parameter.defaultValue;
}

inline void setEffectValue(NativeDspNodeParameters& parameters, const EffectParameter& parameter, const float value) noexcept
{
    if (parameter.field != nullptr)
        parameters.*(parameter.field) = value;
    else if (parameter.slot >= 0 && static_cast<std::size_t>(parameter.slot) < parameters.eqBands.size())
        parameters.eqBands[static_cast<std::size_t>(parameter.slot)] = value;
}

// Note values a time or a rate can follow, shortest first; a sync value of
// 0 is free, n the n-th entry. Lengths in beats.
struct NoteDivision {
    std::string_view name;
    double beats {};
};
inline constexpr std::array<NoteDivision, 12> noteDivisions { {
    { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/8T", 1.0 / 3.0 }, { "1/16.", 0.375 }, { "1/8", 0.5 },
    { "1/4T", 2.0 / 3.0 }, { "1/8.", 0.75 }, { "1/4", 1.0 }, { "1/4.", 1.5 }, { "1/2", 2.0 }, { "1/1", 4.0 },
} };
// The division a sync value selects, or none when it is free.
[[nodiscard]] constexpr const NoteDivision* noteDivisionFor(const float sync) noexcept
{
    const auto index = static_cast<int>(sync + 0.5f);
    return index >= 1 && index <= static_cast<int>(noteDivisions.size()) ? &noteDivisions[static_cast<std::size_t>(index - 1)]
                                                                          : nullptr;
}

// Where an effect sits in the primitive registry.
enum class EffectGroup : std::uint8_t { modulation, delay, filter, dynamics, channels, analysis };

struct EffectDescriptor {
    std::string_view typeKey;
    std::string_view processorKey;
    NativeDspNodeKind kind { NativeDspNodeKind::passthrough };
    std::string_view titleKey;
    std::string_view descriptionKey;
    // English, for the primitive registry and the plug-in exporter.
    std::string_view displayName;
    std::string_view categoryKey;
    EffectGroup group { EffectGroup::modulation };
    std::uint32_t colour {};
    std::span<const EffectParameter> parameters;
    // Its audio input and output: the same width, any (stereo by default),
    // unless it converts one width to another (mono to stereo).
    std::uint8_t inputChannels { 2 };
    std::uint8_t outputChannels { 2 };

    [[nodiscard]] constexpr std::size_t parameterCount() const noexcept { return parameters.size(); }
    [[nodiscard]] constexpr bool convertsChannels() const noexcept { return inputChannels != outputChannels; }
};

// The parametric equaliser: eight bands of six values each, in order, then
// its output gain.
inline constexpr int eqProBands = 8;
inline constexpr int eqProValuesPerBand = 6;
enum class EqProValue : int { enabled, type, frequency, gain, q, slope };
[[nodiscard]] constexpr int eqProSlot(const int band, const EqProValue value) noexcept
{
    return band * eqProValuesPerBand + static_cast<int>(value);
}

namespace effect_detail {
using P = NativeDspNodeParameters;
using K = NativeDspNodeKind;
inline constexpr std::string_view effects = "nodes.category.effects";
inline constexpr std::string_view filters = "nodes.category.filters";
inline constexpr std::string_view dynamics = "nodes.category.dynamics";
inline constexpr std::string_view utilities = "nodes.category.utilities";
inline constexpr std::string_view analyzers = "nodes.category.analyzers";

// Channel conversion: a mono signal placed in the stereo field, and a stereo
// one folded to mono (centre: both halves; the balance leans to one side).
inline constexpr EffectParameter monoToStereo[] {
    { "pan", "monoStereo.pan", "effect.parameter.pan", "", -1.0f, 1.0f, 0.0f, &P::effect1 },
};
inline constexpr EffectParameter stereoToMono[] {
    { "balance", "stereoMono.balance", "effect.parameter.balance", "", -1.0f, 1.0f, 0.0f, &P::effect1 },
};

inline constexpr EffectParameter chorus[] {
    { "rate", "chorus.rate", "effect.parameter.rate", "Hz", 0.05f, 8.0f, 0.6f, &P::effect1 },
    { "depth", "chorus.depth", "effect.parameter.depth", "", 0.0f, 1.0f, 0.5f, &P::effect2 },
    { "delay", "chorus.delay", "effect.parameter.delay", "ms", 2.0f, 40.0f, 14.0f, &P::effect3 },
    { "width", "chorus.width", "effect.parameter.width", "", 0.0f, 1.0f, 0.8f, &P::effect4 },
    { "mix", "chorus.mix", "effect.parameter.mix", "", 0.0f, 1.0f, 0.5f, &P::effect5 },
};
inline constexpr EffectParameter flanger[] {
    { "rate", "flanger.rate", "effect.parameter.rate", "Hz", 0.02f, 5.0f, 0.25f, &P::effect1 },
    { "depth", "flanger.depth", "effect.parameter.depth", "", 0.0f, 1.0f, 0.7f, &P::effect2 },
    { "delay", "flanger.delay", "effect.parameter.delay", "ms", 0.3f, 8.0f, 1.5f, &P::effect3 },
    { "feedback", "flanger.feedback", "effect.parameter.feedback", "", -0.95f, 0.95f, 0.5f, &P::effect4 },
    { "mix", "flanger.mix", "effect.parameter.mix", "", 0.0f, 1.0f, 0.5f, &P::effect5 },
};
inline constexpr EffectParameter phaser[] {
    { "rate", "phaser.rate", "effect.parameter.rate", "Hz", 0.02f, 8.0f, 0.35f, &P::effect1 },
    { "depth", "phaser.depth", "effect.parameter.depth", "", 0.0f, 1.0f, 0.8f, &P::effect2 },
    { "frequency", "phaser.frequency", "effect.parameter.frequency", "Hz", 100.0f, 4'000.0f, 700.0f, &P::effect3 },
    { "feedback", "phaser.feedback", "effect.parameter.feedback", "", 0.0f, 0.9f, 0.5f, &P::effect4 },
    { "mix", "phaser.mix", "effect.parameter.mix", "", 0.0f, 1.0f, 0.5f, &P::effect5 },
};
inline constexpr EffectParameter tremolo[] {
    { "rate", "tremolo.rate", "effect.parameter.rate", "Hz", 0.1f, 20.0f, 4.0f, &P::effect1 },
    { "depth", "tremolo.depth", "effect.parameter.depth", "", 0.0f, 1.0f, 0.6f, &P::effect2 },
    { "shape", "tremolo.shape", "effect.parameter.shape", "", 0.0f, 3.0f, 0.0f, &P::effect3 },
    { "spread", "tremolo.spread", "effect.parameter.spread", "", 0.0f, 1.0f, 0.0f, &P::effect4 },
    { "sync", "tremolo.sync", "effect.parameter.sync", "", 0.0f, 12.0f, 0.0f, &P::effect5 },
};
inline constexpr EffectParameter bitcrusher[] {
    { "bits", "crusher.bits", "effect.parameter.bits", "", 1.0f, 16.0f, 8.0f, &P::effect1 },
    { "downsample", "crusher.downsample", "effect.parameter.downsample", "", 1.0f, 32.0f, 4.0f, &P::effect2 },
    { "drive", "crusher.drive", "effect.parameter.drive", "dB", 0.0f, 24.0f, 0.0f, &P::effect3 },
    { "mix", "crusher.mix", "effect.parameter.mix", "", 0.0f, 1.0f, 1.0f, &P::effect4 },
};
inline constexpr EffectParameter multimodeFilter[] {
    { "mode", "filter.mode", "effect.parameter.mode", "", 0.0f, 3.0f, 0.0f, &P::effect1 },
    { "cutoff", "filter.cutoff", "effect.parameter.cutoff", "Hz", 20.0f, 20'000.0f, 1'000.0f, &P::effect2 },
    { "resonance", "filter.resonance", "effect.parameter.resonance", "", 0.1f, 20.0f, 0.707f, &P::effect3 },
    { "drive", "filter.drive", "effect.parameter.drive", "dB", 0.0f, 24.0f, 0.0f, &P::effect4 },
};
inline constexpr EffectParameter autoFilter[] {
    { "cutoff", "autofilter.cutoff", "effect.parameter.cutoff", "Hz", 40.0f, 15'000.0f, 800.0f, &P::effect1 },
    { "resonance", "autofilter.resonance", "effect.parameter.resonance", "", 0.3f, 10.0f, 2.0f, &P::effect2 },
    { "rate", "autofilter.rate", "effect.parameter.rate", "Hz", 0.05f, 16.0f, 1.0f, &P::effect3 },
    { "depth", "autofilter.depth", "effect.parameter.depth", "", 0.0f, 1.0f, 0.5f, &P::effect4 },
    { "sync", "autofilter.sync", "effect.parameter.sync", "", 0.0f, 12.0f, 0.0f, &P::effect5 },
};
inline constexpr EffectParameter eqThree[] {
    { "low", "eq3.low", "effect.parameter.low", "dB", -24.0f, 12.0f, 0.0f, &P::effect1 },
    { "mid", "eq3.mid", "effect.parameter.mid", "dB", -24.0f, 12.0f, 0.0f, &P::effect2 },
    { "high", "eq3.high", "effect.parameter.high", "dB", -24.0f, 12.0f, 0.0f, &P::effect3 },
    { "low-frequency", "eq3.lowFrequency", "effect.parameter.lowFrequency", "Hz", 50.0f, 800.0f, 250.0f, &P::effect4 },
    { "high-frequency", "eq3.highFrequency", "effect.parameter.highFrequency", "Hz", 1'000.0f, 12'000.0f, 3'500.0f,
      &P::effect5 },
};
inline constexpr EffectParameter limiter[] {
    { "gain", "limiter.gain", "effect.parameter.gain", "dB", 0.0f, 24.0f, 0.0f, &P::effect1 },
    { "ceiling", "limiter.ceiling", "effect.parameter.ceiling", "dB", -24.0f, 0.0f, -0.3f, &P::effect2 },
    { "release", "limiter.release", "effect.parameter.release", "ms", 1.0f, 1'000.0f, 80.0f, &P::effect3 },
};
inline constexpr EffectParameter gate[] {
    { "threshold", "gate.threshold", "effect.parameter.threshold", "dB", -80.0f, 0.0f, -40.0f, &P::effect1 },
    { "attack", "gate.attack", "effect.parameter.attack", "ms", 0.1f, 50.0f, 1.0f, &P::effect2 },
    { "release", "gate.release", "effect.parameter.release", "ms", 5.0f, 2'000.0f, 120.0f, &P::effect3 },
    { "range", "gate.range", "effect.parameter.range", "dB", -80.0f, 0.0f, -80.0f, &P::effect4 },
};
inline constexpr EffectParameter transientShaper[] {
    { "attack", "transient.attack", "effect.parameter.punch", "", -1.0f, 1.0f, 0.0f, &P::effect1 },
    { "sustain", "transient.sustain", "effect.parameter.sustain", "", -1.0f, 1.0f, 0.0f, &P::effect2 },
    { "output", "transient.output", "effect.parameter.output", "dB", -12.0f, 12.0f, 0.0f, &P::effect3 },
};
inline constexpr EffectParameter pingPong[] {
    { "time", "pingpong.time", "effect.parameter.time", "ms", 10.0f, 2'000.0f, 375.0f, &P::effect1 },
    { "feedback", "pingpong.feedback", "effect.parameter.feedback", "", 0.0f, 0.95f, 0.45f, &P::effect2 },
    { "mix", "pingpong.mix", "effect.parameter.mix", "", 0.0f, 1.0f, 0.35f, &P::effect3 },
    { "width", "pingpong.width", "effect.parameter.width", "", 0.0f, 1.0f, 1.0f, &P::effect4 },
    { "sync", "pingpong.sync", "effect.parameter.sync", "", 0.0f, 12.0f, 0.0f, &P::effect5 },
};

// Each band starts off, spread across the spectrum where a first move would
// go, as a bell. Suffixes keep to two names ("eqpro.b3Gain") as every
// other binding does.
#define CYDAW_EQ_BAND(n, hz)                                                                                                    \
    { "b" #n "-enabled", "eqpro.b" #n "Enabled", "effect.parameter.enabled", "", 0.0f, 1.0f, 0.0f, nullptr,               \
      static_cast<std::int16_t>(((n) - 1) * 6 + 0), static_cast<std::int8_t>((n) - 1) },                                   \
    { "b" #n "-type", "eqpro.b" #n "Type", "effect.parameter.type", "", 0.0f, 7.0f, 0.0f, nullptr,                         \
      static_cast<std::int16_t>(((n) - 1) * 6 + 1), static_cast<std::int8_t>((n) - 1) },                                   \
    { "b" #n "-frequency", "eqpro.b" #n "Frequency", "effect.parameter.frequency", "Hz", 10.0f, 22'000.0f, (hz), nullptr,   \
      static_cast<std::int16_t>(((n) - 1) * 6 + 2), static_cast<std::int8_t>((n) - 1) },                                   \
    { "b" #n "-gain", "eqpro.b" #n "Gain", "effect.parameter.gain", "dB", -30.0f, 30.0f, 0.0f, nullptr,                    \
      static_cast<std::int16_t>(((n) - 1) * 6 + 3), static_cast<std::int8_t>((n) - 1) },                                   \
    { "b" #n "-q", "eqpro.b" #n "Q", "effect.parameter.q", "", 0.1f, 30.0f, 1.0f, nullptr,                                 \
      static_cast<std::int16_t>(((n) - 1) * 6 + 4), static_cast<std::int8_t>((n) - 1) },                                   \
    { "b" #n "-slope", "eqpro.b" #n "Slope", "effect.parameter.slope", "", 0.0f, 3.0f, 1.0f, nullptr,                      \
      static_cast<std::int16_t>(((n) - 1) * 6 + 5), static_cast<std::int8_t>((n) - 1) }

// The compressor's detector is high-passed from `sidechain` up: 20 Hz
// leaves it whole.
inline constexpr EffectParameter compressorPro[] {
    { "threshold", "comppro.threshold", "effect.parameter.threshold", "dB", -60.0f, 0.0f, -18.0f, &P::effect1 },
    { "ratio", "comppro.ratio", "effect.parameter.ratio", ":1", 1.0f, 20.0f, 4.0f, &P::effect2 },
    { "knee", "comppro.knee", "effect.parameter.knee", "dB", 0.0f, 24.0f, 6.0f, &P::effect3 },
    { "attack", "comppro.attack", "effect.parameter.attack", "ms", 0.05f, 250.0f, 10.0f, &P::effect4 },
    { "release", "comppro.release", "effect.parameter.release", "ms", 5.0f, 2'500.0f, 120.0f, &P::effect5 },
    { "makeup", "comppro.makeup", "effect.parameter.makeup", "dB", -12.0f, 24.0f, 0.0f, &P::effect6 },
    { "mix", "comppro.mix", "effect.parameter.mix", "", 0.0f, 1.0f, 1.0f, &P::effect7 },
    { "sidechain", "comppro.sidechain", "effect.parameter.sidechain", "Hz", 20.0f, 500.0f, 20.0f, &P::effect8 },
};

// The spectrometer passes its sound untouched: its values are the display's
// (the floor of its scale, the tilt per octave, how smooth the curve, how
// long the peaks stay, how many seconds the waterfall shows).
inline constexpr EffectParameter spectrometer[] {
    { "floor", "spectrometer.floor", "effect.parameter.floor", "dB", -120.0f, -36.0f, -90.0f, &P::effect1 },
    { "tilt", "spectrometer.tilt", "effect.parameter.tilt", "dB", 0.0f, 6.0f, 3.0f, &P::effect2 },
    { "smoothing", "spectrometer.smoothing", "effect.parameter.smoothing", "", 0.0f, 1.0f, 0.5f, &P::effect3 },
    { "hold", "spectrometer.hold", "effect.parameter.hold", "s", 0.0f, 10.0f, 2.0f, &P::effect4 },
    { "history", "spectrometer.history", "effect.parameter.history", "s", 2.0f, 30.0f, 8.0f, &P::effect5 },
};

inline constexpr EffectParameter eqPro[] {
    CYDAW_EQ_BAND(1, 40.0f),    CYDAW_EQ_BAND(2, 120.0f),   CYDAW_EQ_BAND(3, 350.0f),    CYDAW_EQ_BAND(4, 900.0f),
    CYDAW_EQ_BAND(5, 2'200.0f), CYDAW_EQ_BAND(6, 5'000.0f), CYDAW_EQ_BAND(7, 10'000.0f), CYDAW_EQ_BAND(8, 16'000.0f),
    { "output", "eqpro.output", "effect.parameter.output", "dB", -24.0f, 24.0f, 0.0f, &P::effect1 },
};
#undef CYDAW_EQ_BAND
} // namespace effect_detail

inline constexpr std::array<EffectDescriptor, 17> effectCatalogue { {
    { "chorus", "core.chorus", effect_detail::K::chorus, "node.chorus", "node.chorus.description", "Chorus",
      effect_detail::effects, EffectGroup::modulation, 0xff5fd0c4u, effect_detail::chorus },
    { "flanger", "core.flanger", effect_detail::K::flanger, "node.flanger", "node.flanger.description", "Flanger",
      effect_detail::effects, EffectGroup::modulation, 0xff6fb8f0u, effect_detail::flanger },
    { "phaser", "core.phaser", effect_detail::K::phaser, "node.phaser", "node.phaser.description", "Phaser",
      effect_detail::effects, EffectGroup::modulation, 0xff9b8cf2u, effect_detail::phaser },
    { "tremolo-pan", "core.tremolo", effect_detail::K::tremolo, "node.tremoloPan", "node.tremoloPan.description", "Tremolo",
      effect_detail::effects, EffectGroup::modulation, 0xff65d6b1u, effect_detail::tremolo },
    { "bitcrusher", "core.bitcrusher", effect_detail::K::bitcrusher, "node.bitcrusher", "node.bitcrusher.description",
      "Bitcrusher", effect_detail::effects, EffectGroup::dynamics, 0xffff7f6bu, effect_detail::bitcrusher },
    { "multimode-filter", "core.multimode-filter", effect_detail::K::multiFilter, "node.multiFilter",
      "node.multiFilter.description", "Multimode Filter", effect_detail::filters, EffectGroup::filter, 0xff70b7ffu,
      effect_detail::multimodeFilter },
    { "auto-filter", "core.auto-filter", effect_detail::K::autoFilter, "node.autoFilter", "node.autoFilter.description",
      "Auto Filter", effect_detail::filters, EffectGroup::filter, 0xff5fc9d8u, effect_detail::autoFilter },
    { "eq-three", "core.eq-three", effect_detail::K::threeBandEq, "node.eqThree", "node.eqThree.description", "EQ Three",
      effect_detail::filters, EffectGroup::filter, 0xff5fc9d8u, effect_detail::eqThree },
    { "eq-pro", "core.eq-pro", effect_detail::K::parametricEq, "node.eqPro", "node.eqPro.description", "Parametric EQ",
      effect_detail::filters, EffectGroup::filter, 0xff4fd6e8u, effect_detail::eqPro },
    { "comp-pro", "core.comp-pro", effect_detail::K::compressorPro, "node.compPro", "node.compPro.description",
      "Pro Compressor", effect_detail::dynamics, EffectGroup::dynamics, 0xffff9a3cu, effect_detail::compressorPro },
    { "limiter", "core.limiter", effect_detail::K::limiter, "node.limiter", "node.limiter.description", "Limiter",
      effect_detail::dynamics, EffectGroup::dynamics, 0xffffcf5cu, effect_detail::limiter },
    { "gate", "core.gate", effect_detail::K::gate, "node.gate", "node.gate.description", "Gate", effect_detail::dynamics,
      EffectGroup::dynamics, 0xffe8b45cu, effect_detail::gate },
    { "transient-shaper", "core.transient-shaper", effect_detail::K::transientShaper, "node.transientShaper",
      "node.transientShaper.description", "Transient Shaper", effect_detail::dynamics, EffectGroup::dynamics, 0xfff0a43au,
      effect_detail::transientShaper },
    { "ping-pong", "core.ping-pong", effect_detail::K::pingPongDelay, "node.pingPong", "node.pingPong.description",
      "Ping-Pong Delay", effect_detail::effects, EffectGroup::delay, 0xffffa547u, effect_detail::pingPong },
    { "mono-to-stereo", "core.mono-to-stereo", effect_detail::K::monoToStereo, "node.monoToStereo",
      "node.monoToStereo.description", "Mono to Stereo", effect_detail::utilities, EffectGroup::channels, 0xff8fb6d8u,
      effect_detail::monoToStereo, 1, 2 },
    { "stereo-to-mono", "core.stereo-to-mono", effect_detail::K::stereoToMono, "node.stereoToMono",
      "node.stereoToMono.description", "Stereo to Mono", effect_detail::utilities, EffectGroup::channels, 0xff8fb6d8u,
      effect_detail::stereoToMono, 2, 1 },
    { "spectrometer", "core.spectrometer", effect_detail::K::spectrometer, "node.spectrometer",
      "node.spectrometer.description", "Spectrometer", effect_detail::analyzers, EffectGroup::analysis, 0xff6cb8ffu,
      effect_detail::spectrometer },
} };

[[nodiscard]] constexpr const EffectDescriptor* effectForKind(const NativeDspNodeKind kind) noexcept
{
    for (const auto& effect : effectCatalogue)
        if (effect.kind == kind)
            return &effect;
    return nullptr;
}

[[nodiscard]] constexpr const EffectDescriptor* effectForProcessor(const std::string_view processorKey) noexcept
{
    for (const auto& effect : effectCatalogue)
        if (effect.processorKey == processorKey)
            return &effect;
    return nullptr;
}

[[nodiscard]] constexpr const EffectDescriptor* effectForType(const std::string_view typeKey) noexcept
{
    for (const auto& effect : effectCatalogue)
        if (effect.typeKey == typeKey)
            return &effect;
    return nullptr;
}

[[nodiscard]] constexpr const EffectParameter* effectParameter(const EffectDescriptor& effect,
                                                                const std::string_view key) noexcept
{
    for (const auto& parameter : effect.parameters)
        if (!parameter.key.empty() && parameter.key == key)
            return &parameter;
    return nullptr;
}

// A binding suffix ("chorus.rate") belongs to one effect only.
[[nodiscard]] constexpr const EffectParameter* effectParameterForSuffix(const std::string_view suffix,
                                                                         const EffectDescriptor** owner = nullptr) noexcept
{
    for (const auto& effect : effectCatalogue)
        for (const auto& parameter : effect.parameters)
            if (!parameter.suffix.empty() && parameter.suffix == suffix) {
                if (owner != nullptr)
                    *owner = &effect;
                return &parameter;
            }
    return nullptr;
}

// The payload with an effect's defaults written into its fields.
inline void applyEffectDefaults(const EffectDescriptor& effect, NativeDspNodeParameters& parameters) noexcept
{
    for (const auto& parameter : effect.parameters)
        setEffectValue(parameters, parameter, parameter.defaultValue);
}

} // namespace cydaw::engine
