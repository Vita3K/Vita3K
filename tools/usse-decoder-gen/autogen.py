import yaml
import string
import sys
import re
from glob import glob

# Need Python 3.6 or newer for stable insertion order dicts
assert sys.version_info >= (3, 6), 'Needs Python 3.6 or newer'

# Set this to true to indent the annotation string in line with the match string
STAIRCASE_INDENT = True

def allocate_char(chars_used, name):
    # Pick a character from the identifier
    for c in name:
        if c not in chars_used and c in string.ascii_letters:
            return c

    # Fallback to the first free alphabetic character
    c = 'a'
    while c in chars_used:
        if c == 'z':
            c = 'A'
        else:
            c = chr(ord(c) + 1)
    return c


funcdefs = []
matchers = []

d = yaml.safe_load(open('grammar.yaml'))

for k, v in d.items():
    members = []
    description = v.get('description', '')
    offset = 64
    for member in v['members']:
        for mk, mv in member.items():
            if isinstance(mv, str):
                # Literal binary match
                mv = {'size': len(mv), 'match': mv}
            elif isinstance(mv, int):
                # Simple bit field
                mv = {'size': mv}

            if 'type' not in mv:
                mv['type'] = f'Imm{mv["size"]}'

            if 'offset' in mv:
                # Re-adjust offset and potentially insert DONTCARE bits
                new_offset = mv['offset'] + mv['size']
                assert new_offset <= offset, f'Offset {new_offset} must be less or equal to current offset {offset}'
                dontcare_bits = offset - new_offset
                if dontcare_bits != 0:
                    members.append(
                        ('DONTCARE', 64-offset, {'size': dontcare_bits, 'type': f'Imm{dontcare_bits}'}))
                    offset = new_offset

            members.append((mk, 64-offset, mv))
            offset -= mv['size']

    # Insert DONTCARE bits at the end (if any)
    if offset > 0:
        dontcare_bits = offset
        members.append(
            ('DONTCARE', 64-offset, {'size': dontcare_bits, 'type': f'Imm{dontcare_bits}'}))
        offset = 0

    assert offset == 0, f'Definition of {k} does not fit in 64 bits'

    v["handler"] = k.lower()
    args = ',\n    '.join(f'{props["type"]} {name}' for name,
                          offset, props in members if name != 'DONTCARE' and 'match' not in props)
    func = (v["handler"], args)
    funcdefs.append(func)

    # Create match string and assign bit characters
    matchstr = ''
    annotations = []
    chars_used = {}
    PREFIX = f'INST(&V::{v["handler"]}, "{k} ()", "'
    SUFFIX = '"),'
    indent = len(PREFIX)
    for name, offset, props in members:
        indentation = ' ' * (indent + offset) if STAIRCASE_INDENT else '  '
        size = props['size']
        if name == 'DONTCARE':
            matchstr += '-' * size
            annotations.append(f'{indentation}{"-"*size} = don\'t care')
        elif 'match' in props:
            # Literal bit pattern match
            matchstr += props['match']
            annotations.append(f'{indentation}{props["match"]} = {name}')
        else:
            char = allocate_char(chars_used, name)
            chars_used[char] = name
            matchstr += char * size
            bitinfo = '1 bit' if size == 1 else f'{size} bits'
            arginfo = '' if props['type'].startswith(
                'Imm') else f', {props["type"]}'
            annotations.append(
                f'{indentation}{char*size} = {name} ({bitinfo}{arginfo})')

    annotation = '\n'.join(annotations)
    matchers.append(f'// {description}\n/*\n{annotation}\n*/\n{PREFIX}{matchstr}{SUFFIX}')

def replace_file(file, pat, replace):
    with open(file, 'r') as f:
        content = f.read()
        if isinstance(pat, list):
            for p, r in zip(pat,replace):
                content_new = re.sub(p, r, content)
                content = content_new
        else:
            content_new = re.sub(pat, replace, content)
    with open(file, 'w') as f:
        f.write(content_new)

def dump(fp):
    print('// Functions\n', file=fp)
    for funcdef in funcdefs:
        print(funcdef, file=fp)
        print('', file=fp)

    print('\n\n// Matchers\n', file=fp)
    for matcher in matchers:
        print(matcher, file=fp)
        print('', file=fp)

def update_matcher():
    entry_pat = r'(static const std::vector<USSEMatcher<V>> table = {)[^\}]+(})'
    out = ""
    for matcher in matchers:
        out += matcher
        out += '\n'
    out = '\n'.join(['        ' + x for x in out.splitlines()])
    out = """
#define INST(fn, name, bitstring) shader::decoder::detail::detail<USSEMatcher<V>>::GetMatcher(fn, name, bitstring)
        // clang-format off
""" + out + "\n        // clang-format on\n"
    replace_file('../../vita3k/shader/src/usse_translator_entry.cpp', entry_pat, r'\1'+out+r'    \2')

def update_visitor():
    # update headers
    header_pat = r'(// Instructions start)[^/]+(// Instructions end)'
    out = '\n'
    for func in funcdefs:
        out += f'bool {func[0]}({func[1]});\n\n'
    def tab(x):
        if x != '':
            return '    ' + x
        return ''
    out = '\n'.join([tab(x) for x in out.splitlines()])
    replace_file('../../vita3k/shader/include/shader/usse_translator.h', header_pat, r'\1'+out+r'    \2')

    # update sources
    def src_pat(name):
        return r'(bool USSETranslatorVisitor::' + name + r'\()[^)]*(\))'
    
    files = glob('../../vita3k/shader/src/translator/*.cpp')
    for file in files:
        replace_file(file, [src_pat(func[0]) for func in funcdefs], [r'\1\n    '+func[1]+r'\2' for func in funcdefs])

update_matcher()
update_visitor()
