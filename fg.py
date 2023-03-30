#!/usr/bin/env python3
"""

"""

from __future__ import annotations
from pathlib import Path
import os
from dataclasses import dataclass, field
import functools
import re


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
):
    g = dotdict()
    g.env = {
        'FG_BUFFER_COUNT': '0'
    }
    g.current_shader = None
    g.fg_shaders = {}
    g.fg_scratch = []

    g._scratch_current_binding = 0
    g._scratch_current_offset = 0

    def ERROR(*args, **kwargs):
        raise ValueError(f'ERROR: {args = !r}; {kwargs = !r}')

    def EMIT(line):
        g.fg_shaders[g.current_shader].append(line)
    
    @specialize(ERROR)
    def APPEND(prefix, **kwargs):
        assert prefix == 'FG_BUFFER_'
        count = int(g.env[f'{prefix}_COUNT'])

        g.env[f'{prefix}_COUNT'] = str(count + 1)
        for key, value in kwargs.items():
            g.env[f'{prefix}_{key}_{count}'] = str(value)
    
    g._scratch_atomic_first = True
    g._scratch_atomic_binding = 0
    g._scratch_atomic_offset = 0
    @specialize(ERROR)
    def SCRATCH(type, name, count, line):
        assert type == 'atomic_uint'
        assert count is None

        # if g._scratch_atomic_first:
        #     APPEND('FG_BUFFER_', KIND='ATOMICS', BINDING='0')
        #     g._scratch_atomic_first = False

        EMIT(f'layout (binding={g._scratch_atomic_binding}, offset={g._scratch_atomic_offset}) uniform {type} {name}; // {line}')

        g._scratch_atomic_offset += 4
        
    g._ssbo_binding = 0
    @specialize(SCRATCH)
    def SCRATCH(type, name, count, line):
        assert type in ['uint']
        assert count == 'N'

        EMIT(f'layout (std430, binding={g._ssbo_binding}) buffer _{name} {{ {type} {name}[]; }}; // line')
        APPEND('FG_BUFFER_', KIND='SCRATCH_NODE_UINT', NAME=f'_{name}')

        g._ssbo_binding += 1
    
    def ATTRIBUTE(type, name, count, line):
        assert type in ['float', 'uint']
        assert count == 'N'

        EMIT(f'layout (std430, binding={g._ssbo_binding}) buffer _{name} {{ {type} {name}[]; }}; // {line}')
        APPEND('FG_BUFFER_', KIND='ATTRIBUTE_NODE', NAME=f'_name')

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
    
    SHADER('common')
    with open(input_filename, 'rt') as f:
        for line in f:
            line = line.rstrip()
            LINE(line)

    print(f'--- common')
    print("\n".join(g.fg_shaders['common']))

    print(f'--- positional')
    print("\n".join(g.fg_shaders['positional']))

    print(f'--- relational')
    print("\n".join(g.fg_shaders['relational']))

    print(f'--- appearance')
    print("\n".join(g.fg_shaders['appearance']))

    # env = {
    #     'FG_SHADER_COMMON': '',
    #     'FG_SHADER_VERTEX': '',
    #     'FG_SHADER_GEOMETRY': '',
    #     'FG_SHADER_FRAGMENT': '',
    # }

    # os.execlpe(executable, f'{executable.name} <{input_filename.name}>', env)


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--input', '-i', dest='input_filename', type=Path, required=True)
    parser.add_argument('--executable', '-x', dest='executable', type=Path, default=Path('fg'))
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
