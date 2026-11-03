"""Generate the host CyUI catalogue from the existing presentation map."""
import argparse
import ast
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def generate():
    catalog={}
    for french,english in json.loads((ROOT/'tools/english_strings.json').read_text(encoding='utf-8')).items():
        try:
            french=ast.literal_eval('"'+french+'"');english=ast.literal_eval('"'+english+'"')
        except (SyntaxError,ValueError):
            continue
        if '\0' not in english and not english.startswith(('//@','#include')):
            catalog.setdefault(english,french)
    catalog.update({'Appearance':'Apparence','Theme':'Thème','Density':'Densité','Compact':'Compacte','Comfortable':'Confortable','Language':'Langue','Frames / second':'Images / seconde'})
    text='// Generated from tools/english_strings.json; English remains the default.\n#pragma once\n#include <cyui/Locale.h>\nnamespace sf { inline void registerUiTranslations() {\n    cyui::registerTranslations("fr",{\n'
    for en,fr in sorted(catalog.items()):
        text+='        {'+json.dumps(en,ensure_ascii=False)+','+json.dumps(fr,ensure_ascii=False)+'},\n'
    return text+'    },"Français");\n} }\n'
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    target=ROOT/'src/ui/Translations.h';text=generate()
    if args.check:
        if target.read_text(encoding='utf-8')!=text:raise SystemExit('Stale host translation catalogue')
        print('Host translation catalogue verified')
    else:
        target.write_text(text,encoding='utf-8',newline='\n');print('Host translation catalogue generated')
if __name__=='__main__':main()
