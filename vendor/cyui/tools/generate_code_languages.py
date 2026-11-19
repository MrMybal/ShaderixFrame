"""Regenerate the shared catalogue from pinned, permissively licensed lexer sources.

Inputs stay untouched. Outputs are deterministic C++ tables and the selected vendor
subset; no Notepad++ source or GPL configuration is copied.
"""
import argparse
import fnmatch
import json
from pathlib import Path
import re
import shutil


def properties(path):
    data = {}
    pending = ''
    for raw in path.read_text('utf-8-sig', errors='replace').splitlines():
        line = pending + raw.strip()
        if line.endswith('\\'):
            pending = line[:-1]; continue
        pending = ''
        if not line or line.startswith('#') or '=' not in line: continue
        key, value = line.split('=', 1)
        data[key.strip()] = value.strip()
    return data


def expand(value, values, stack=()):
    def replace(match):
        key = match[1]
        return '' if key in stack else expand(values.get(key, ''), values, (*stack, key))
    return re.sub(r'\$\(([^)]+)\)', replace, value)


def literal(value):
    # MSVC has a per-literal size limit. Adjacent literals preserve the exact data.
    return '\n'.join(json.dumps(value[i:i+4000], ensure_ascii=True) for i in range(0, len(value), 4000)) or '""'


def generate(root, lexilla, scintilla, scite):
    catalogue = json.loads((root/'data/code-languages.json').read_text('utf-8'))
    symbols = {}
    for source in sorted((lexilla/'lexers').glob('*.cxx')):
        content = source.read_text('utf-8')
        for symbol, constant, name in re.findall(r'LexerModule\s+(lm\w+)\s*\(\s*(SCLEX_\w+)\s*,.*?"([\w+-]+)"', content, re.S):
            symbols[name] = (symbol, constant, source)
    values = {}
    for source in sorted((scite/'src').glob('*.properties')):
        values.update(properties(source))
    lexer_prefixes = {}
    for constant, prefixes in re.findall(r'^lex \w+=(SCLEX_\w+) (.+)$', (lexilla/'include/LexicalStyles.iface').read_text('utf-8'), re.M):
        lexer_prefixes[constant] = prefixes.split()
    styles = re.findall(r'^#define (SCE_\w+) (\d+)$', (lexilla/'include/SciLexer.h').read_text('utf-8'), re.M)
    selected = sorted({item['lexer'] for item in catalogue if item.get('lexer')})
    missing = set(selected)-symbols.keys()
    if missing: raise ValueError(f'Missing Lexilla engines: {missing}')
    folder = root/'src/modules/code'; folder.mkdir(parents=True, exist_ok=True)
    generated = ['// Generated from the pinned Lexilla/SciTE sources. See third_party/code-syntax.lock.json.',
                 '#include "CodeSyntaxInternal.h"', '#include "ILexer.h"', '#include "LexerModule.h"']
    for name in selected:
        generated.append(f'extern const Lexilla::LexerModule {symbols[name][0]};')
    generated.append('namespace cyui { namespace detail {')
    for name in selected:
        prefixes = lexer_prefixes.get(symbols[name][1], lexer_prefixes.get('SCLEX_HTML',[]) if name=='phpscript' else [])
        names = ['']*256
        for style, index in styles:
            if int(index)<256 and any(style.startswith(prefix) for prefix in prefixes): names[int(index)] = style
        generated.append(f'static const std::array<const char*,256> styles_{name}={{{{'+','.join(literal(s) for s in names)+'}};')
    generated.append('const std::vector<LanguageProfile>& builtinProfiles() { static const std::vector<LanguageProfile> profiles={')
    for item in catalogue:
        if item.get('native'):
            if item['native'] != 'verse' or item.get('lexer'):
                raise ValueError('Unknown native syntax profile')
            info='{CodeLanguage::'+item['id']+','+literal(item['id'])+','+literal(item['name'])+', {'+','.join(literal(x) for x in item['extensions'])+'},{'+','.join(literal(x) for x in item['filenames'])+'},true}'
            generated.append('{'+info+', nullptr, {}, {}, nullptr, {}},')
            continue
        local = dict(values)
        tests = lexilla/'test/examples'/item['lexer']/'SciTE.properties'
        if tests.exists(): local.update(properties(tests))
        lists = ['']*9
        props_name={'cpp':'cpp','hypertext':'html','phpscript':'html','xml':'html','props':'others','batch':'others','diff':'others','makefile':'others','bash':'others','errorlist':'others','COBOL':'cobol','f77':'fortran'}.get(item['lexer'],item['lexer'])
        preferred=scite/'src'/(props_name+'.properties')
        sheets=[values]
        if preferred.exists(): sheets.append(properties(preferred))
        if tests.exists(): sheets.append(properties(tests))
        for key, value in [(k,v) for sheet in sheets for k,v in sheet.items()]:
            key = expand(key, local)
            match = re.fullmatch(r'keywords([2-9]?)\.(.+)', key)
            if match and any(fnmatch.fnmatchcase(item['sample'], pattern) for pattern in match[2].split(';')):
                lists[int(match[1] or '1')-1] = ' '.join(expand(value, local).split())
        settings = {k:expand(v,local) for k,v in local.items() if k.startswith(('lexer.', 'fold.')) and not any(x in k for x in ('$', '*', ';'))}
        settings.update({'fold':'1','lexer.cpp.track.preprocessor':'0','lexer.cpp.styling.within.preprocessor':'1'})
        settings.update(item.get('properties', {}))
        if item['id']=='asp': settings['asp.default.language']='2'
        if item['id']=='jsp': settings['asp.default.language']='3'
        if item['id']=='json': settings['lexer.json.allow.comments']='1'
        if item['id']=='scheme': lists[0]='define lambda if else cond let let* letrec begin set! quote quasiquote unquote and or case do delay'
        for index, text in item.get('wordLists', {}).items(): lists[int(index)] = text
        functions = item.get('functions', '').split()
        info='{CodeLanguage::'+item['id']+','+literal(item['id'])+','+literal(item['name'])+', {'+','.join(literal(x) for x in item['extensions'])+'},{'+','.join(literal(x) for x in item['filenames'])+'},true}'
        generated.append('{'+info+', &::'+symbols[item['lexer']][0]+', {'+','.join(literal(x) for x in lists)+'}, {'+
                         ','.join('{'+literal(k)+','+literal(v)+'}' for k,v in sorted(settings.items()))+'}, &styles_'+item['lexer']+', {'+
                         ','.join(literal(x) for x in functions)+'}},')
    generated.append('}; return profiles; }\n} }')
    (folder/'LanguageData.cpp').write_text('\n'.join(generated)+'\n', 'utf-8')
    enum = root/'include/cyui/detail/CodeLanguageList.inc'; enum.parent.mkdir(parents=True,exist_ok=True)
    enum.write_text('// Generated; existing values 0..13 are stable.\n'+''.join(f'{item["id"]} = {index},\n' for index,item in enumerate(catalogue)), 'utf-8')
    vendor = root/'third_party/lexilla'; (vendor/'lexers').mkdir(parents=True,exist_ok=True)
    shutil.copytree(lexilla/'lexlib', vendor/'lexlib', dirs_exist_ok=True)
    (vendor/'include').mkdir(exist_ok=True)
    for name in ('SciLexer.h','LexicalStyles.iface'): shutil.copy2(lexilla/'include'/name,vendor/'include'/name)
    shutil.copy2(lexilla/'License.txt', vendor/'License.txt')
    for source in {symbols[name][2] for name in selected}: shutil.copy2(source,vendor/'lexers'/source.name)
    shutil.copytree(scintilla/'include',root/'third_party/scintilla/include',dirs_exist_ok=True)
    shutil.copy2(scintilla/'License.txt',root/'third_party/scintilla/License.txt')
    shutil.copy2(scite/'License.txt', root/'third_party/SciTE-Properties-License.txt')
    print(f'{len(catalogue)} syntax modes, {len(selected)} engines, {len(set(symbols[name][2] for name in selected))} lexer source units')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    for name in ('lexilla','scintilla','scite'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();generate(args.root,args.lexilla,args.scintilla,args.scite)
