#!/usr/bin/env python3
"""

"""

from __future__ import annotations
from pathlib import Path
import os
from dataclasses import dataclass, field
import functools
import re
import copy


def specialize(next_function):
    def wrapper(curr_function):
        @functools.wraps(curr_function)
        def inner_wrapper(*args, **kwargs):
            try:
                ret = curr_function(*args, **kwargs)
            except AssertionError:
                ret = Ellipsis
            
            if ret is Ellipsis:
                ret = next_function(*args, **kwargs)

            return ret
        return inner_wrapper
    return wrapper


class dotdict(dict):
    """dot.notation access to dictionary attributes"""
    __getattr__ = dict.get
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__


@dataclass
class ScratchList:
    _current_binding: int = field(default=0)
    _current_offset: int = field(default=0)


def main(
    *,
    input_filename: Path,
    executable: Path,
    datafiles: List[Tuple[str, str]],
):
    datafiles: Dict[str, str] = { x[0]: x[1] for x in datafiles }

    g = dotdict()
    # g.env = copy.copy(os.environ)
    g.env = {}
    g.env['FG_TILE_WIDTH'] = '256'
    g.env['FG_TILE_HEIGHT'] = '256'
    g.env['FG_BUFFER_COUNT'] = '0'
    g.env['FG_TILE_Z'] = '1'
    g.env['FG_TILE_X'] = '1'
    g.env['FG_TILE_Y'] = '0'
    g.env['FG_OUTPUT'] = 'out.jpg'
    g.current_shader = None
    g.fg_shaders = {}
    g.fg_scratch = []

    g._scratch_current_binding = 0
    g._scratch_current_offset = 0

    def ERROR(*args, **kwargs):
        raise ValueError(f'ERROR: {args = !r}; {kwargs = !r}')
    
    _OPENGL_ = {
        'uint': 'GL_UNSIGNED_INT',
        'float': 'GL_FLOAT',
    }
    @specialize(ERROR)
    def OPENGL(s):
        assert s in _OPENGL_
        return _OPENGL_[s]

    def EMIT(line):
        g.fg_shaders[g.current_shader].append(line)
    
    @specialize(ERROR)
    def APPEND(prefix, **kwargs):
        assert prefix == 'FG_BUFFER'
        count = int(g.env[f'{prefix}_COUNT'])

        g.env[f'{prefix}_COUNT'] = str(count + 1)
        for key, value in kwargs.items():
            g.env[f'{prefix}_{key}_{count}'] = str(value)
    
    g._scratch_atomic_exists = False
    g._scratch_atomic_binding = 0
    g._scratch_atomic_offset = 0
    @specialize(ERROR)
    def SCRATCH(type, name, count, line):
        assert type == 'atomic_uint'
        assert count is None

        EMIT(f'layout (binding={g._scratch_atomic_binding}, offset={g._scratch_atomic_offset}) uniform {type} {name}; // {line}')

        g._scratch_atomic_exists = True
        g._scratch_atomic_offset += 4
        
    g._ssbo_binding = 1
    @specialize(SCRATCH)
    def SCRATCH(type, name, count, line):
        assert type in ['uint']
        assert count == 'N'

        EMIT(f'layout (std430, binding={g._ssbo_binding}) buffer _{name} {{ {type} {name}[]; }}; // {line}')
        APPEND('FG_BUFFER',
            KIND='SCRATCH',
            NAME=f'_{name}',
            TYPE=OPENGL(type),
            SIZE=count,
            FILE='<NONE>',
        )

        g._ssbo_binding += 1
    
    def ATTRIBUTE(type, name, count, line):
        assert type in ['float', 'uint']
        assert count == 'N'

        EMIT(f'layout (std430, binding={g._ssbo_binding}) buffer _{name} {{ {type} {name}[]; }}; // {line}')
        APPEND('FG_BUFFER',
            KIND='ATTRIBUTE',
            NAME=f'_{name}',
            TYPE=OPENGL(type),
            SIZE=count,
            FILE=datafiles[name],
        )

        g._ssbo_binding += 1
    
    def SHADER(name):
        g.current_shader = name
        g.fg_shaders[name] = []
    
    @specialize(EMIT)
    def LINE(line):
        match = re.match(r'^\s*#\s*pragma\s+fg\s+(?P<line>.+?)\s*$', line)
        assert match is not None
        
        line = match.group('line')
        PRAGMA(line)

    @specialize(ERROR)
    def PRAGMA(line):
        match = re.match(r'scratch\s*\(\s*(?P<type>\S+)\s+(?P<name>\S+?)\s*(?:\[(?P<count>\S+?)\])?\)\s*$', line)
        assert match is not None

        type = match.group('type')
        name = match.group('name')
        count = match.group('count')
        SCRATCH(type, name, count, line)
    
    @specialize(PRAGMA)
    def PRAGMA(line):
        match = re.match(r'attribute\s*\(\s*(?P<type>\S+)\s+(?P<name>\S+?)(?:\[(?P<count>\S+?)\])?\)\s*$', line)
        assert match is not None

        type = match.group('type')
        name = match.group('name')
        count = match.group('count')
        ATTRIBUTE(type, name, count, line)

    @specialize(PRAGMA)
    def PRAGMA(line):
        match = re.match(r'shader\s*\(\s*(?P<name>\S+?)\)\s*$', line)
        assert match is not None

        name = match.group('name')
        SHADER(name)
    
    APPEND('FG_BUFFER',
        KIND='ELEMENT',
        NAME='<NONE>',
        TYPE=OPENGL('uint'),
        SIZE='2E',
        FILE=datafiles['element'],
    )

    SHADER('common')
    with open(input_filename, 'rt') as f:
        for line in f:
            line = line.rstrip()
            LINE(line)
    
    if g._scratch_atomic_exists:
        APPEND('FG_BUFFER',
            KIND='ATOMIC',
            NAME='<NONE>',
            TYPE=OPENGL('uint'),
            SIZE=g._scratch_atomic_offset,
            FILE='<NONE>',
        )

    g.fg_shaders['common'] = "\n".join(g.fg_shaders['common'])
    g.fg_shaders['positional'] = "\n".join(g.fg_shaders['positional'])
    g.fg_shaders['relational'] = "\n".join(g.fg_shaders['relational'])
    g.fg_shaders['appearance'] = "\n".join(g.fg_shaders['appearance'])

    g.env['FG_SHADER_COMMON'] = f'''
#version 460 core
precision mediump float;

uniform float uTranslateX;
uniform float uTranslateY;
uniform float uScale;
{g.fg_shaders['common']}
    '''

    g.env['FG_SHADER_VERTEX'] = f'''

out uint _fg_NodeIndex;

void main() {{
    const int fg_NodeIndex = gl_VertexID;
    vec3 fg_NodePosition = vec3(0., 0., 0.);

{g.fg_shaders['positional']}

    gl_Position = vec4(fg_NodePosition.xyz, 1.);

//     // Apply tile transformations
//     gl_Position.xy *= uScale;
//     gl_Position.xy += vec2(uTranslateX, uTranslateY);

//     // Transform xy in [0, 1] to xy in [-1, 1]
//     gl_Position.xy *= 2.;
//     gl_Position.xy -= 1.;

    _fg_NodeIndex = fg_NodeIndex;
}}
    '''

    g.env['FG_SHADER_GEOMETRY'] = f'''
layout (lines) in;
layout (line_strip, max_vertices=2) out;

in uint _fg_NodeIndex[];

void main() {{
    const uint fg_SourceIndex = _fg_NodeIndex[0];
    const uint fg_TargetIndex = _fg_NodeIndex[1];

{g.fg_shaders['relational']}

    gl_PrimitiveID = 2 * gl_PrimitiveIDIn + 0;
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();

    gl_Position = gl_in[1].gl_Position;
    EmitVertex();

    EndPrimitive();

    gl_PrimitiveID = 2 * gl_PrimitiveIDIn + 1;
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();

    gl_Position = gl_in[1].gl_Position;
    EmitVertex();

    EndPrimitive();
}}
    '''

    g.env['FG_SHADER_FRAGMENT'] = f'''
out vec4 fg_FragColor;

void main() {{
    const bool fg_IsSource = gl_PrimitiveID % 2 == 0;
    const int fg_PrimitiveID = gl_PrimitiveID / 2;

{g.fg_shaders['appearance']}
}}
    '''

    file = executable
    arg0 = f'{executable} <{input_filename.name}>'
    env = g.env
    print(f'os.execlpe({file!r}, {arg0!r}, env)')
    for k, v in env.items():
        print(f'env[{k}]:\n---8<---\n{v}\n--->8---')

    os.execlpe(executable, arg0, env)


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--input', '-i', dest='input_filename', type=Path, required=True)
    parser.add_argument('--executable', '-x', dest='executable', default=Path('fg'))
    parser.add_argument('--file', '-f', dest='datafiles', action='append', nargs=2, default=[])
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
