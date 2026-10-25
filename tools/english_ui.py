"""English presentation for generated shaders/widgets; serialized IDs remain intact."""
import json
import re
from pathlib import Path

STRINGS = json.loads(Path(__file__).with_name('english_strings.json').read_text(encoding='utf-8'))
CPP_TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|R"(?P<tag>\w*)\(.*?\)(?P=tag)"|(?P<string>(?:L|u8)?"(?:\\.|[^"\\])*")', re.S)

def english(text):
    return STRINGS.get(text, text)

def translate_cpp(text):
    def replace(match):
        token = match.group('string')
        if token is None:
            return match[0]
        quote = token.index('"')
        return token[:quote + 1] + english(token[quote + 1:-1]) + '"'
    return CPP_TOKEN.sub(replace, text)

def translate_shader(text):
    text = re.sub(r'(?m)^(//@(?:name|category|group)\s+)([^\r\n]+)', lambda m: m[1] + english(m[2]), text)
    def parameter(match):
        return re.sub(r'"([^"\n]*)"', lambda m: '"' + '|'.join(english(part) for part in m[1].split('|')) + '"', match[0])
    return re.sub(r'(?m)^//@param[^\r\n]+', parameter, text)
