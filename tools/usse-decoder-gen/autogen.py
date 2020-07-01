import yaml
import string
import sys


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

d = yaml.load(open('grammar.yaml'))

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
    func = f'bool USSETranslatorVisitor::{v["handler"]}(\n    {args})\n{{\n}}'
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


def dump(fp):
    print('// Functions\n', file=fp)
    for funcdef in funcdefs:
        print(funcdef, file=fp)
        print('', file=fp)

    print('\n\n// Matchers\n', file=fp)
    for matcher in matchers:
        print(matcher, file=fp)
        print('', file=fp)


dump(sys.stdout)
dump(open('autogen_out.cpp', 'w'))
