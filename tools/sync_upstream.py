"""Read-only upstream audit and explicit, hash-guarded snapshot updates.

python tools/sync_upstream.py --bootstrap
python tools/sync_upstream.py --check
python tools/sync_upstream.py --update --component shaderixel
python tools/sync_upstream.py --update --component cydaw2 --only include/cydaw/engine/HaloReverb.h ...
Never modifies either upstream. Locally modified vendored files are refused.
"""
import argparse
import hashlib
import json
from pathlib import Path
from upstream_paths import source_roots, acceptable_hashes

ROOT = Path(__file__).resolve().parents[1]
SOURCES = source_roots()
FILES = {
    'shaderixel': ['src/core/Common.h', 'src/core/Json.h', 'src/core/Json.cpp',
                  'src/doc/EffectGraph.h', 'src/doc/EffectGraph.cpp',
                  'src/ui/Theme.h', 'src/ui/Theme.cpp'],
    'cydaw2': ['LICENSE', 'include/cydaw/plugins/PluginIsolation.h', 'src/plugin_worker/PluginWorkerMain.cpp', 'build/desktop-asio/CyDAWPluginWorker_artefacts/Release/CyDAWPluginWorker.exe', 'include/cydaw/graph/GraphDefinition.h', 'src/graph/GraphDefinition.cpp',
               'include/cydaw/engine/AudioBuffer.h', 'src/engine/AudioBuffer.cpp',
               'include/cydaw/engine/AudioAnalysis.h', 'src/engine/AudioAnalysis.cpp',
               'include/cydaw/engine/EffectProcessors.h', 'src/engine/EffectProcessors.cpp',
               'include/cydaw/engine/EffectCatalogue.h', 'include/cydaw/engine/NativeDspGraph.h',
               'include/cydaw/engine/BiquadDesign.h', 'include/cydaw/engine/AudioProcessor.h',
               'include/cydaw/engine/Transport.h', 'include/cydaw/engine/Sampler.h',
               'include/cydaw/engine/SoundModel.h', 'include/cydaw/engine/WavetableShape.h',
               'src/v2/ArrangementView.cpp', 'src/v2/ArrangementView.h',
               'src/v2/Theme.h'],
    'imgui': ['imgui.h', 'imconfig.h', 'imgui_internal.h', 'imgui.cpp', 'imgui_draw.cpp',
              'imgui_tables.cpp', 'imgui_widgets.cpp', 'imstb_rectpack.h',
              'imstb_textedit.h', 'imstb_truetype.h', 'LICENSE.txt',
              'backends/imgui_impl_dx11.h', 'backends/imgui_impl_dx11.cpp',
              'backends/imgui_impl_win32.h', 'backends/imgui_impl_win32.cpp',
              'misc/cpp/imgui_stdlib.h', 'misc/cpp/imgui_stdlib.cpp'],
}
FILES['shaderixel'] += ['shaders/effects/'+p.name for p in sorted((SOURCES['shaderixel']/'shaders/effects').glob('*.slang'))]
SOURCES['imgui'] = SOURCES['shaderixel'] / 'external/imgui'
FILES['juce-license'] = ['LICENSE.md']
FILES['cydaw2'] += ['build/desktop-asio/CyDAWTestGain_artefacts/Release/VST3/CyDAW Test Gain.vst3/Contents/Resources/moduleinfo.json',
                   'build/desktop-asio/CyDAWTestGain_artefacts/Release/VST3/CyDAW Test Gain.vst3/Contents/x86_64-win/CyDAW Test Gain.vst3']
FILES['cydaw2'] += ['src/v2/GraphEditor.h','src/v2/GraphEditor.cpp','src/v2/GraphLayout.h',
                   'src/v2/GraphLayout.cpp','src/v2/Widgets.h','src/v2/Widgets.cpp',
                   'src/app/AudioTrackController.h','src/app/AudioTrackController.cpp']

FILES['shaderixel'] += ['src/doc/Document.h','src/doc/Document.cpp','src/ui/NodeEditor.cpp',
                       'shaders/blend_modes.slang','shaders/composite.slang','shaders/common.slang']
FILES['cydaw2'] += ['include/cydaw/engine/AudioGraph.h','include/cydaw/engine/AudioRenderPlan.h',
                   'src/engine/AudioGraph.cpp','src/engine/AudioRenderPlan.cpp',
                   'src/v2/Theme.cpp','src/v2/MixerView.cpp','src/v2/MixerView.h','resources/i18n/fr.json']

# CyDAW's stereo insert effects (reverbs, delays, dynamics, amp/cab, pitch, utility). PrismSynth and
# KickSynth are linked for the default values of NativeDspNodeParameters only; containers stay out
# (src/adapters/CyDawUnavailable.cpp stands in for their factory).
CYDAW_EFFECTS = ['AddedEffects', 'AmpDesign', 'AmpEffects', 'Convolver', 'HaloReverb', 'MixEffects',
                 'MotionEffects', 'PitchEffects', 'ProEffects', 'ResonanceEffects', 'RhythmEffects',
                 'SpaceEffects', 'SuiteEffects', 'UtilityEffects', 'VoiceEffects', 'KickSynth', 'PrismSynth']
CYDAW_EFFECT_HEADERS = CYDAW_EFFECTS + ['BuiltinProcessors', 'ContainerProcessors', 'Containers', 'DrumKit', 'Fft',
                                        'HaloDesign', 'InstrumentContainers', 'MidiEffects', 'MidiEffectsDesign',
                                        'MixDesign', 'MotionDesign', 'PitchDesign', 'PitchTracker', 'PolySynthGraph',
                                        'PolyVoice', 'PolyVoiceFilter', 'PolyVoiceOscillator', 'ProEffectsDesign',
                                        'ResonanceDesign', 'RhythmDesign', 'SpaceDesign', 'SuiteDesign', 'SynthFamily',
                                        'UtilityDesign', 'VoiceDesign', 'VoiceLive']
FILES['cydaw2'] += ['src/engine/' + name + '.cpp' for name in CYDAW_EFFECTS]
FILES['cydaw2'] += sorted({'include/cydaw/engine/' + name + '.h' for name in CYDAW_EFFECT_HEADERS if name != 'Convolver'} |
                          {'include/cydaw/engine/Convolver.h'})
FILES['cydaw2'] += ['include/cydaw/realtime/RtPublisher.h', 'src/engine/DspParts.h']

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--bootstrap', action='store_true')
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--update', action='store_true')
    parser.add_argument('--component', choices=FILES)
    parser.add_argument('--only', nargs='+', metavar='PATH',
                        help='limit an update to these paths of the component (same hash guards)')
    args = parser.parse_args()
    if args.only and not (args.update and args.component):
        raise SystemExit('--only needs --update and --component.')
    lock_path = ROOT / 'upstream.lock.json'
    lock = json.loads(lock_path.read_text('utf-8')) if lock_path.exists() else {'schema': 1, 'files': {}}
    if args.bootstrap and lock['files']:
        raise SystemExit('Already bootstrapped; use --check or --update.')
    pending = []
    changed = False
    for component, paths in FILES.items():
        if args.component and component != args.component:
            continue
        for relative in paths:
            if args.only and relative not in args.only:
                continue
            source = SOURCES[component] / relative
            target = ROOT / 'vendor' / component / relative
            key = f'{component}/{relative}'
            data = source.read_bytes()
            current = digest(data)
            previous = lock['files'].get(key, {}).get('sha256')
            if target.exists() and previous and digest(target.read_bytes()) not in acceptable_hashes(key, lock['files'][key]):
                raise SystemExit(f'Local vendor edit refused: {key}. Move changes into an adapter first.')
            if previous != current or not target.exists():
                changed = True
                print(f'CHANGED {key}')
                pending.append((key, target, data, current, relative))
    if args.check:
        print('Upstream changes available.' if changed else 'All upstream snapshots match.')
        return 1 if changed else 0
    # Validate all inputs before writing anything. Historical snapshots remain recoverable.
    for key, target, data, sha, source in pending:
        if target.exists():
            old = target.read_bytes()
            backup = ROOT / 'vendor-history' / digest(old) / key
            backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(old)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        lock['files'][key] = {'source': source, 'sha256': sha}
        if key.startswith('cydaw2/build/'):
            lock['files'][key]['externalArtifact'] = True
    lock_path.write_text(json.dumps(lock, indent=2) + '\n', encoding='utf-8')
    print(f'Snapshotted {len(pending)} files. Rebuild and run tests before accepting compatibility.')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
