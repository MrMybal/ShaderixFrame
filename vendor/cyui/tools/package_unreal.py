"""Prepare a reusable Unreal Editor module or its optional plugin container.

No engine files or existing projects are changed. Output must be a new directory.
Run the resulting plugin with UnrealBuildTool, not the CyUI CMake build.
"""
from pathlib import Path
import argparse
import shutil
import json
import re
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEPENDENCIES = {
    'Audio': [], 'Canvas': [], 'Plots': ['Canvas'], 'Curves': ['Plots'],
    'Nodes': ['Canvas'], 'Timeline': ['Canvas'], 'AudioViews': ['Plots'],
    'Code': [], 'Diff': ['Code'], 'CodeEditor': ['Code'], 'Inspector': [],
    'Assets': [], 'Images': ['Canvas'], 'Viewport': ['Images'], 'Commands': [],
    'Diagnostics': [], 'DocumentViews': ['Code', 'Images'], 'DocumentEditor': ['DocumentViews', 'CodeEditor', 'DataGrid'],
    'DocumentIO': ['DocumentViews'], 'DocumentWeb': ['DocumentViews', 'Web'], 'Media': ['Images'], 'Web': [], 'DataGrid': [], 'Spreadsheet': ['DataGrid'],
    'RichDocument': ['DocumentEditor', 'DataGrid'], 'OfficeIO': ['RichDocument', 'Spreadsheet', 'DocumentIO'],
    'Hierarchy': [], 'DataView': ['Inspector'], 'FilePicker': [],
}
# Inside the checkout, output may only go to these ignored build folders.
OUTPUT_FOLDERS = ('build', 'out')


def select_modules(names):
    selected = set()
    def require(name):
        if name == 'Core':
            return
        if name not in DEPENDENCIES:
            raise ValueError(f'Unknown module: {name}')
        if name in selected:
            return
        for dependency in DEPENDENCIES[name]:
            require(dependency)
        selected.add(name)
    for name in names:
        for item in (DEPENDENCIES if name == 'All' else [name]):
            require(item)
    return sorted(selected)


def validate_output(output):
    output = Path(output).resolve()
    # Refuse overwrites, symlinks and partial replacement of an installed plugin.
    if output.exists() or output.is_symlink():
        raise FileExistsError(f'Output already exists: {output}. Choose a new directory.')
    # Anything else under the checkout may be a copied tree (docs, include...).
    if output == ROOT or ROOT in output.parents and output.relative_to(ROOT).parts[0].casefold() not in OUTPUT_FOLDERS:
        raise ValueError('Output inside the CyUI checkout must be under build/ or out/')
    return output


def validate_module_name(name):
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,63}', name or ''):
        raise ValueError(f'Invalid Unreal module name: {name!r}')
    return name


def rename_module(output, name):
    """Renames Source/CyUI to a unique module: folder, Build.cs class and file, descriptor and
    test names. The C++ takes its module name and CYUI_API from the Build.cs definitions."""
    source = output/'Source/CyUI'
    rules = source/'CyUI.Build.cs'
    text = rules.read_text(encoding='utf8')
    for old in ('public class CyUI : ModuleRules', 'public CyUI(ReadOnlyTargetRules Target)'):
        if old not in text:
            raise RuntimeError(f'Unexpected Build.cs template: {old!r} not found')
        text = text.replace(old, old.replace('CyUI', name))
    (source/f'{name}.Build.cs').write_text(text, encoding='utf8')
    rules.unlink()
    tests = source/'Private/Tests/CyUIEditorTests.cpp'
    tests.write_text(tests.read_text(encoding='utf8').replace('"CyUI.Editor.', f'"{name}.Editor.'), encoding='utf8')
    source.rename(output/'Source'/name)
    descriptor = output/'CyUI.uplugin'
    data = json.loads(descriptor.read_text(encoding='utf8'))
    for module in data['Modules']:
        if module['Name'] == 'CyUI':
            module['Name'] = name
    descriptor.write_text(json.dumps(data, indent=2)+'\n', encoding='utf8')


def prepare_plugin(output, modules=('Core',), gallery=False, module_name='CyUI'):
    output = validate_output(output)
    module_name = validate_module_name(module_name)
    selected = select_modules(modules)
    shutil.copytree(ROOT/'integrations/unreal/CyUI', output)
    library = output/'ThirdParty/CyUI'
    shutil.copytree(ROOT/'include', library/'include')
    shutil.copytree(ROOT/'src/fonts', library/'src/fonts')
    shutil.copytree(ROOT/'third_party/jetbrains-mono', library/'third_party/jetbrains-mono')
    sources = ['src/Theme.cpp', 'src/Widgets.cpp', 'src/Layout.cpp', 'src/Filter.cpp', 'src/Numeric.cpp']
    sources += [f'src/modules/{name}.cpp' for name in selected]
    support = {'Inspector': 'InspectorData', 'Nodes': 'NodeData', 'Timeline': 'TimelineData', 'Commands': 'CommandRegistry'}
    sources += [f'src/modules/{unit}.cpp' for module, unit in support.items() if module in selected]
    if 'CodeEditor' in selected:
        sources.append('src/modules/PagedCodeEditor.cpp')
    if 'DocumentViews' in selected:
        sources += ['src/modules/DocumentParser.cpp', 'third_party/md4c/md4c.cpp']
    if 'DocumentIO' in selected:
        sources.append('third_party/miniz/miniz_tinfl.cpp')
    if 'OfficeIO' in selected:
        sources.append('third_party/pugixml/pugixml.cpp')
    if gallery:
        sources.append('src/Gallery.cpp')
    (library/'src/modules').mkdir(parents=True)
    shutil.copy2(ROOT/'src/modules/ModuleInternal.h', library/'src/modules/ModuleInternal.h')
    for source in sources:
        target = library/source
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT/source, target)
    if 'DocumentViews' in selected:
        shutil.copytree(ROOT/'third_party/md4c', library/'third_party/md4c', dirs_exist_ok=True)
        for name in ('DocumentLayout.h', 'DocumentEntities.h'):
            shutil.copy2(ROOT/'src/modules'/name, library/'src/modules'/name)
    if 'DocumentIO' in selected:
        shutil.copytree(ROOT/'third_party/miniz', library/'third_party/miniz', dirs_exist_ok=True)
    if 'OfficeIO' in selected:
        shutil.copytree(ROOT/'third_party/pugixml', library/'third_party/pugixml', dirs_exist_ok=True)
    imgui = library/'third_party/imgui'
    imgui.mkdir(parents=True)
    for name in ('imgui.h', 'imgui_internal.h', 'imconfig.h', 'imstb_rectpack.h', 'imstb_textedit.h', 'imstb_truetype.h', 'LICENSE.txt'):
        shutil.copy2(ROOT/'third_party/imgui'/name, imgui/name)
    for name in ('imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets'):
        source = f'third_party/imgui/{name}.cpp'
        shutil.copy2(ROOT/source, library/source)
        sources.append(source)
    if 'Code' in selected:
        shutil.copytree(ROOT/'data', library/'data')
        syntax = ROOT/'src/modules/code'
        shutil.copytree(syntax, library/'src/modules/code')
        sources += [source.relative_to(ROOT).as_posix() for source in sorted(syntax.glob('*.cpp'))]
        for name in ('lexilla', 'scintilla'):
            shutil.copytree(ROOT/'third_party'/name, library/'third_party'/name)
        for name in ('code-syntax.lock.json', 'SciTE-Properties-License.txt'):
            shutil.copy2(ROOT/'third_party'/name, library/'third_party'/name)
        sources += [source.relative_to(ROOT).as_posix() for source in sorted((ROOT/'third_party/lexilla').rglob('*.cxx'))]
    if 'CodeEditor' in selected:
        shutil.copytree(ROOT/'third_party/texteditor', library/'third_party/texteditor')
        sources.append('third_party/texteditor/TextEditor.cpp')
    private = output/'Source/CyUI/Private'
    # UBT promotes these MSVC diagnostics to errors. Keep allowances local to
    # the bridge that includes the corresponding vendor or generated code.
    source_warnings = {
        'third_party/md4c/md4c.cpp': '4668 4800 4702',  # Unreachable fallback after MD_UNREACHABLE().
        'src/modules/code/LanguageData.cpp': '4883',  # Large generated initializer.
    }
    for index, source in enumerate(sources):
        vendor = source.startswith('third_party/') or source == 'src/modules/CodeEditor.cpp'
        warnings = source_warnings.get(source, '4668 4800' if vendor else '')
        (private/f'CyUILibrary_{index:02}_{Path(source).stem}.cpp').write_text(
            '// Generated source bridge; the implementation remains in ThirdParty/CyUI.\n'
            '#include "CoreTypes.h"\n'
            + (f'#if defined(_MSC_VER)\n#pragma warning(push)\n#pragma warning(disable: {warnings})\n#endif\n' if warnings else '')
            + f'#include "../../../ThirdParty/CyUI/{source}"\n'
            + ('#if defined(_MSC_VER)\n#pragma warning(pop)\n#endif\n' if warnings else ''), encoding='utf8')
    rules = output/'Source/CyUI/CyUI.Build.cs'
    features = selected + (['Gallery'] if gallery else [])
    rules.write_text(rules.read_text(encoding='utf8').replace('/*CYUI_MODULES*/', ', '.join('"'+s.upper()+'"' for s in features)), encoding='utf8')
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(ROOT/name, output/name)
        shutil.copy2(ROOT/name, library/name)
    notices = output/'THIRD_PARTY_NOTICES.md'
    notices.write_text(notices.read_text(encoding='utf8').replace('(third_party/', '(ThirdParty/CyUI/third_party/'), encoding='utf8')
    (output/'.gitattributes').write_text('ThirdParty/CyUI/third_party/** -text !eol\n', encoding='utf8')
    shutil.copytree(ROOT/'Documentation', library/'Documentation')
    shutil.copytree(ROOT/'docs', library/'docs', ignore=shutil.ignore_patterns('screenshots'))
    (output/'Config').mkdir(exist_ok=True)
    (output/'Config/FilterPlugin.ini').write_text('[FilterPlugin]\n/ThirdParty/...\n/LICENSE\n/THIRD_PARTY_NOTICES.md\n', encoding='utf8')
    if module_name != 'CyUI':
        rename_module(output, module_name)
    return selected


def prepare_module(output, modules=('Core',), gallery=False, module_name='CyUI'):
    """A relocatable Source/CyUI folder, with no .uplugin or outside files required.

    UBT discovers .cpp recursively inside a module. Library implementations are
    therefore stored byte-for-byte as .inl and compiled only by source bridges.
    """
    output = validate_output(output)
    module_name = validate_module_name(module_name)
    with tempfile.TemporaryDirectory(prefix='cyui-module-') as temporary:
        staging = Path(temporary)/'CyUI'
        selected = prepare_plugin(staging, modules, gallery, module_name)
        shutil.copytree(staging/'Source'/module_name, output)
        library = output/'Library'
        shutil.copytree(staging/'ThirdParty/CyUI', library)
        sources = {}
        for source in sorted([*library.rglob('*.cpp'), *library.rglob('*.cxx')]):
            target = source.with_suffix('.inl')
            sources[source.relative_to(library).as_posix()] = target.relative_to(library).as_posix()
            source.rename(target)
        for bridge in (output/'Private').glob('CyUILibrary_*.cpp'):
            text = bridge.read_text(encoding='utf8')
            for original, target in sources.items():
                text = text.replace('../../../ThirdParty/CyUI/'+original, '../Library/'+target)
            bridge.write_text(text, encoding='utf8')
        for dependency in ('texteditor', 'pugixml'):
            version = library/'third_party'/dependency/'VERSION.json'
            if version.exists():
                data = json.loads(version.read_text(encoding='utf8'))
                renamed = {}
                for name in list(data['files']):
                    original = f'third_party/{dependency}/{name}'
                    if original in sources:
                        packaged = Path(sources[original]).name
                        data['files'][packaged] = data['files'].pop(name)
                        renamed[name] = packaged
                if renamed:
                    data['packaged_paths'] = renamed
                    version.write_text(json.dumps(data, indent=2)+'\n', encoding='utf8')
        (library/'SOURCE_LAYOUT.json').write_text(json.dumps(sources, indent=2)+'\n', encoding='utf8')
        for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
            shutil.copy2(ROOT/name, output/name)
        notices = output/'THIRD_PARTY_NOTICES.md'
        notices.write_text(notices.read_text(encoding='utf8').replace('(third_party/', '(Library/third_party/'), encoding='utf8')
        (output/'.gitattributes').write_text('Library/third_party/** -text !eol\n', encoding='utf8')
        (output/'ModuleDescriptor.json').write_text(json.dumps({
            'Name': module_name, 'Type': 'Editor', 'LoadingPhase': 'PostEngineInit'
        }, indent=2)+'\n', encoding='utf8')
    return selected


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='New Source/CyUI directory or plugin staging directory')
    parser.add_argument('--layout', choices=['module', 'plugin'], default='module', help='Default: reusable module to embed in an existing plugin')
    parser.add_argument('--modules', nargs='+', default=['Core'], choices=['Core', 'All', *DEPENDENCIES])
    parser.add_argument('--gallery', action='store_true')
    parser.add_argument('--module-name', default='CyUI', help='Unique module name for a copy embedded in a redistributed plugin, e.g. MyToolCyUI')
    args = parser.parse_args()
    prepare = prepare_module if args.layout == 'module' else prepare_plugin
    chosen = prepare(args.output, args.modules, args.gallery, args.module_name)
    print('Prepared CyUI editor '+args.layout+' '+args.module_name+' with Core'+(', '+', '.join(chosen) if chosen else '')+(' and Gallery' if args.gallery else ''))
