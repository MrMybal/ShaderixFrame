#pragma once

#include <cydaw/engine/Sampler.h>
#include <cydaw/engine/SoundModel.h>
#include <cydaw/engine/WavetableShape.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cydaw::engine {

enum class NativeDspNodeKind : std::uint8_t {
    passthrough,
    input,
    output,
    drive,
    gain,
    lowpass,
    highpass,
    hardClip,
    stereoWidth,
    dcBlocker,
    oscillator,
    noise,
    delay,
    reverb,
    compressor,
    equalizer,
    analyzer,
    reverbRack,
    vocalStrip,
    mixer,
    // Appended to preserve the serialized numeric identity of every existing
    // native node kind.
    pan,
    envelopeFollower,
    controlConstant,
    controlLfo,
    controlAdd,
    controlSubtract,
    controlMultiply,
    controlDivide,
    controlMinimum,
    controlMaximum,
    controlClamp,
    controlAbsolute,
    controlNegate,
    controlMapRange,
    controlScaleOffset,
    controlGreater,
    controlLess,
    controlEqual,
    controlAnd,
    controlOr,
    controlXor,
    controlNot,
    // New values are always appended: serialized graphs store this enum as a
    // stable integer and older node identities must never move.
    controlModulo,
    controlPower,
    controlSine,
    controlCosine,
    controlSquareRoot,
    controlFloor,
    controlCeil,
    controlRound,
    peakingEq,
    wavetable,
    stringResonator,
    // The effect catalogue (EffectCatalogue.h); appended, as every kind is.
    chorus,
    flanger,
    phaser,
    tremolo,
    bitcrusher,
    multiFilter,
    autoFilter,
    threeBandEq,
    limiter,
    gate,
    transientShaper,
    pingPongDelay,
    parametricEq,
    compressorPro,
    monoToStereo,
    stereoToMono,
    // An analyser: the sound passes untouched, the display reads it.
    spectrometer
};

// A compact, dependency-free parameter payload that travels with a graph node.
// Fields that are not used by a given node kind keep their defaults. Keeping the
// values in the render description makes presets, copied tracks and reusable
// modules deterministic instead of depending on UI-global controls.
struct NativeDspNodeParameters {
    float driveDb {};
    float gainDb {};
    float cutoffHz { 20'000.0f };
    float resonance { 0.707f };
    float highpassCutoffHz { 20.0f };
    float clipThreshold { 1.0f };
    float stereoWidth { 1.0f };
    float pan {};
    float oscillatorFrequencyHz { 220.0f };
    float noiseLevel { 0.06f };
    float delayMilliseconds { 260.0f };
    float feedback { 0.32f };
    float mix { 0.28f };
    float roomSize { 0.68f };
    float thresholdDb { -16.0f };
    float ratio { 4.0f };
    float tone { 0.18f };
    // One parametric EQ band: an RBJ peaking biquad. Several in series make
    // the mixing EQ the single-knob tone control could never be.
    float bandFrequencyHz { 1'000.0f };
    float bandGainDb {};
    float bandQ { 1.0f };
    // Where the wavetable reads between its frames, and how far its unison
    // voices are spread.
    float wavetablePosition { 0.35f };
    float wavetableDetuneCents { 8.0f };
    // A waveguide string: how fast the loop loses its highs, and how long the
    // note takes to die away.
    float stringDamping { 0.35f };
    float stringDecaySeconds { 4.0f };
    float oscillatorWaveform {};
    float oscillatorLevel { 0.14f };
    float attackMilliseconds { 8.0f };
    float decayMilliseconds { 120.0f };
    float sustainLevel { 0.72f };
    float releaseMilliseconds { 280.0f };
    // Control-rate graph parameters. They live in the common node payload so
    // value graphs use the same preset, nesting and serialisation path as DSP
    // graphs, without taking part in the audio render plan.
    float modulationRateHz { 1.0f };
    float modulationDepth { 1.0f };
    float modulationOffset {};
    float modulationShape {};
    // General value and event operands used by control/MIDI primitives. These
    // stay in the shared payload so editable graphs, nested definitions and
    // presets all use one deterministic serialisation path.
    float valueOperand {};
    // Unconnected Value sockets retain independent authored defaults, just
    // like MetaSound input pins. A cable always takes precedence at runtime.
    float valueInputA {};
    float valueInputB {};
    float valueMinimum { -1.0f };
    float valueMaximum { 1.0f };
    float mapInputMinimum {};
    float mapInputMaximum { 1.0f };
    float mapOutputMinimum {};
    float mapOutputMaximum { 1.0f };
    float followerAttackSeconds { 0.01f };
    float followerReleaseSeconds { 0.1f };
    float midiTranspose {};
    float midiMinimumNote {};
    float midiMaximumNote { 127.0f };
    float midiVelocityScale { 1.0f };
    // The effect catalogue's values: which field means what is the entry's
    // business (EffectCatalogue.h).
    float effect1 {};
    float effect2 {};
    float effect3 {};
    float effect4 {};
    float effect5 {};
    float effect6 {};
    float effect7 {};
    float effect8 {};
    // The parametric equaliser's bands, six values each.
    std::array<float, 48> eqBands {};
    // A sound-model card's settings, in SoundModelSetting order.
    SoundModelValues soundModel { defaultSoundModelValues() };
    // A sampler card's settings, in SamplerSetting order.
    SamplerValues sampler { defaultSamplerValues() };

    [[nodiscard]] bool operator==(const NativeDspNodeParameters&) const = default;
};

struct NativeDspNode {
    std::uint64_t id {};
    NativeDspNodeKind kind { NativeDspNodeKind::passthrough };
    NativeDspNodeParameters parameters;
    // The table a wavetable node was shaped with, shared rather than copied.
    // It sits beside the parameter payload rather than inside it: that
    // payload is walked field by field as floats, which is what lets the
    // suite prove every one of them survives a save.
    WavetableBankRef wavetable;
    // An input node: which of the graph's audio inputs it receives (0 the
    // main one; 1, 2… the others a track is routed into, a sidechain).
    std::uint32_t inputBus {};

    [[nodiscard]] bool operator==(const NativeDspNode&) const = default;
};

struct NativeDspConnection {
    std::uint64_t source {};
    std::uint64_t destination {};
    // Stable authored socket IDs survive expansion and VST state export. The
    // ProcessSpec carries the exact main-stream width (Mono through 7.1), while
    // retaining both endpoints preserves future multi-bus routing semantics
    // such as sidechains and independently addressable surround buses.
    std::uint64_t sourcePort {};
    std::uint64_t destinationPort {};

    [[nodiscard]] bool operator==(const NativeDspConnection&) const = default;
};

// Control-rate cable retained beside the sample-rate audio render plan. A
// connection whose destinationParameterKey is empty feeds another control
// primitive. A non-empty key binds the source directly to an atomic parameter
// of an audio processor (for example "gain-db", "cutoff" or "pan").
struct NativeDspModulationConnection {
    std::uint64_t source {};
    std::uint64_t destination {};
    std::uint64_t sourcePort {};
    std::uint64_t destinationPort {};
    std::string destinationParameterKey;
    // Set (0): the cable's value is the parameter's. Modulate (1): the
    // parameter keeps its value (knob, automation) and the cable moves it
    // around it, by `amount` of its range per unit (an LFO's -1..1 sweeps
    // +-amount), on a logarithmic scale for a range that spans decades.
    std::uint8_t mode {};
    float amount { 0.5f };
    float minimum {};
    float maximum { 1.0f };

    [[nodiscard]] bool operator==(
        const NativeDspModulationConnection&) const = default;
};

struct NativeDspGraph {
    std::vector<NativeDspNode> nodes;
    std::vector<NativeDspConnection> connections;
    std::vector<NativeDspModulationConnection> modulations;
    std::uint64_t outputNodeId {};

    [[nodiscard]] bool operator==(const NativeDspGraph&) const = default;
};

} // namespace cydaw::engine
