"""

"""

from base64 import b64encode, b64decode
from collections import namedtuple
from dataclasses import dataclass
from io import StringIO
import itertools
from pathlib import Path
from pprint import pprint
import re
import sys
from urllib.parse import urlparse, urlunparse

from jinja2 import Environment, FileSystemLoader
from pcpp import Preprocessor

from functools import partial
# p = partial(print, file=sys.stderr, flush=True)
p = lambda *args: None


def pairwise(iterable):
    "s -> (s0,s1), (s1,s2), (s2, s3), ..."
    a, b = itertools.tee(iterable)
    next(b, None)
    return zip(a, b)


def flatten(list_of_lists):
    "Flatten one level of nesting"
    return itertools.chain.from_iterable(list_of_lists)


def grouper(iterable, n, fillvalue=None):
    "Collect data into fixed-length chunks or blocks"
    # grouper('ABCDEFG', 3, 'x') --> ABC DEF Gxx"
    args = [iter(iterable)] * n
    return itertools.zip_longest(*args, fillvalue=fillvalue)


def b64encode(s, *, b64encode=b64encode):
    return b64encode(s.encode('utf-8')).decode('utf-8')


def b64decode(s, *, b64decode=b64decode):
    return b64decode(s.encode('utf-8')).decode('utf-8')


def preprocess(source):
    preprocessor = Preprocessor()
    preprocessor.line_directive = None
    preprocessor.parse(source)

    stream = StringIO()
    preprocessor.write(stream)

    return stream.getvalue()


def main_node(infile, outfile):
    filename = infile.name
    source = infile.read()

    env = Environment(
        loader=FileSystemLoader(Path(__file__).parent),
    )
    template = env.get_template('vert.glsl.template')
    source = template.render(dict(
        ShaderType=ShaderType,
        StorageQualifier=StorageQualifier,
        source=source,
        preprocessed=False,
    ))
    source = preprocess(source)
    ast = parse(source, filename, shader_type=ShaderType.Vertex)

    for function in ast.functions:
        if function.name == 'node':
            break
    else:
        print(f'Expected function of name "node": {ast.functions = }', file=sys.stderr)
        return

    in_params = []
    out_params = []
    for parameter in function.parameters:
        if parameter.storage == StorageQualifier.In.value:
            in_params.append(parameter)

        elif parameter.storage == StorageQualifier.Out.value:
            out_params.append(parameter)

        else:
            print(f'Unexpected storage value: {parameter.storage = }', file=sys.stderr)
            return

    outfile.write(template.render(dict(
        ShaderType=ShaderType,
        StorageQualifier=StorageQualifier,
        source=source,
        in_params=in_params,
        out_params=out_params,
        preprocessed=True,
    )))


def main_edge(infile, outfile):
    filename = infile.name
    source = infile.read()

    env = Environment(
        loader=FileSystemLoader(Path(__file__).parent),
    )
    template = env.get_template('frag.glsl.template')
    source = template.render(dict(
        ShaderType=ShaderType,
        StorageQualifier=StorageQualifier,
        source=source,
        preprocessed=False,
    ))
    p(f'{source = !s}')
    source = preprocess(source)
    p(f'{source = !s}')
    ast = parse(source, filename, shader_type=ShaderType.Fragment)
    p(f'ast')

    for function in ast.functions:
        if function.name == 'edge':
            break
    else:
        print(f'Expected function of name "edge": {ast.functions = }', file=sys.stderr)
        return

    in_params = []
    out_params = []
    for parameter in function.parameters:
        if parameter.storage == StorageQualifier.In.value:
            in_params.append(parameter)

        elif parameter.storage == StorageQualifier.Out.value:
            out_params.append(parameter)

        else:
            print(f'Unexpected storage value: {parameter.storage = }', file=sys.stderr)
            return

    outfile.write(template.render(dict(
        ShaderType=ShaderType,
        StorageQualifier=StorageQualifier,
        source=source,
        in_params=in_params,
        out_params=out_params,
        preprocessed=True,
    )))


def main_makeurl(infile, nodefile, edgefile, outfile):
    if nodefile is not None:
        nodeshader = StringIO()
        main_node(nodefile, nodeshader)
        nodeshader = nodeshader.getvalue()

    # print(f'{nodeshader = !s}', file=sys.stderr)

    if edgefile is not None:
        edgeshader = StringIO()
        main_edge(edgefile, edgeshader)
        edgeshader = edgeshader.getvalue()

    # print(f'{edgeshader = !s}', file=sys.stderr)

    for url in infile:
        url = url.rstrip()
        scheme, netloc, path, params, query, fragment = urlparse(url)
        empty, tile, dataset, z, x, y, options = path.split('/', 6)
        z, x, y = map(int, (z, x, y))
        options = { k: v for k, v in grouper(options.split(',')[1:], 2) }

        # print(f'{b64decode(options["vert"][len("base64:"):]) =!s}', file=sys.stderr)
        # print(f'{b64decode(options["frag"][len("base64:"):]) =!s}', file=sys.stderr)

        options['vert'] = f'base64:{b64encode(nodeshader)}'
        options['frag'] = f'base64:{b64encode(edgeshader)}'

        options = ',' + ','.join(flatten(options.items()))
        z, x, y = map(str, (z, x, y))
        path = '/'.join((empty, tile, dataset, z, x, y, options))
        url = urlunparse((scheme, netloc, path, params, query, fragment))

        print(url, file=outfile)


def main_makeurl_repl(infile, outfile):
    p('got here 1')
    p(repr(infile))
    for url in infile:
        p('got here 2')
        print(f'got {url = }', file=sys.stderr, flush=True)
        url = url.rstrip()
        scheme, netloc, path, params, query, fragment = urlparse(url)
        empty, tile, dataset, z, x, y, options = path.split('/', 6)
        z, x, y = map(int, (z, x, y))
        options = { k: v for k, v in grouper(options.split(',')[1:], 2) }

        p('got here 3')

        p(f'{b64decode(options["vert"][len("base64:"):]) =!s}')
        p(f'{b64decode(options["frag"][len("base64:"):]) =!s}')
        
        nodefile = StringIO(b64decode(options["vert"][len("base64:"):]))
        nodefile.name = '<url>'
        nodeshader = StringIO()
        try:
            main_node(nodefile, nodeshader)
        except Exception as e:
            print('<wtf>', file=outfile, flush=True)
            import traceback; traceback.print_exc(file=sys.stderr)
            continue
        nodeshader = nodeshader.getvalue()

        p('done w/ node')

        edgefile = StringIO(b64decode(options["frag"][len("base64:"):]))
        edgefile.name = '<url>'
        edgeshader = StringIO()
        try:
            main_edge(edgefile, edgeshader)
        except Exception as e:
            print('<wtfer>', file=outfile, flush=True)
            import traceback; traceback.print_exc(file=sys.stderr)
            continue
        edgeshader = edgeshader.getvalue()

        p('done w/ edge')

        options['vert'] = f'base64:{b64encode(nodeshader)}'
        options['frag'] = f'base64:{b64encode(edgeshader)}'

        # print(f'{b64decode(options["vert"][len("base64:"):]) =!s}', file=sys.stderr)
        # print(f'{b64decode(options["frag"][len("base64:"):]) =!s}', file=sys.stderr)

        options = ',' + ','.join(flatten(options.items()))
        z, x, y = map(str, (z, x, y))
        path = '/'.join((empty, tile, dataset, z, x, y, options))
        url = urlunparse((scheme, netloc, path, params, query, fragment))

        print(url, file=outfile, flush=True)

    print("main function exited? this should not happen.", file=sys.stderr, flush=True)


Parameter = namedtuple('Parameter', 'storage qualifier type name')


def main():
    env = Environment(
        loader=FileSystemLoader(Path(__file__).parent),
    )
    frag_template = env.get_template('frag.glsl.template')
    vert_template = env.get_template('vert.glsl.template')

    it = iter(sys.stdin)
    it = (x.rstrip() for x in it)
    for url in it:
        if url == '':
            break

        scheme, netloc, path, params, query, fragment = urlparse(url)
        empty, tile, dataset, z, x, y, options = path.split('/', 6)
        z, x, y = map(int, (z, x, y))
        options = { k: v for k, v in grouper(options.split(',')[1:], 2) }


        vert = b64decode(options["vert"][len("base64:"):])
        p(f'{vert = !s}')

        vert = vert_template.render(dict(
            source=vert,
            preprocessed=False,
        ))
        p(f'{vert = !s}')

        vert = preprocess(vert)
        p(f'{vert = !s}')

        match = re.search(r'void node\(.+', vert)
        if match is None:
            print('', flush=True)
            continue
        node_def = match.group()
        p(f'{node_def = }')

        node_params = []
        for match in re.finditer(r'(?P<storage>in|out)\s+(?:(?P<qualifier>flat)\s+)?(?P<type>float|vec4|vec2|vec3|int)\s+(?P<name>\w+)', node_def):
            node_params.append(Parameter(
                storage=match.group('storage'),
                qualifier=match.group('qualifier'),
                type=match.group('type'),
                name=match.group('name'),
            ))
        p(f'{node_params = }')

        vert = vert_template.render(dict(
            source=vert,
            preprocessed=True,
            params=node_params,
        ))
        p(f'{vert = !s}')

        options['vert'] = f'base64:{b64encode(vert)}'


        frag = b64decode(options["frag"][len("base64:"):])
        p(f'{frag = !s}')

        frag = frag_template.render(dict(
            source=frag,
            preprocessed=False,
        ))
        p(f'{frag = !s}')

        frag = preprocess(frag)
        p(f'{frag = !s}')

        match = re.search(r'void edge\(.+', frag)
        if match is None:
            print('', flush=True)
            continue
        edge_def = match.group()
        p(f'{edge_def = }')

        edge_params = []
        for match in re.finditer(r'(?P<storage>in|out)\s+(?:(?P<qualifier>flat)\s+)?(?P<type>float|vec4|vec2|vec3|int)\s+(?P<name>\w+)', edge_def):
            edge_params.append(Parameter(
                storage=match.group('storage'),
                qualifier=match.group('qualifier'),
                type=match.group('type'),
                name=match.group('name'),
            ))
        p(f'{edge_params = }')

        node_param_names = [param.name for param in node_params]
        node_param_names += [
            'fg_FragCoord',
            'fg_FragDepth',
            'fg_EdgeData',
        ]

        params_from_node = []
        params_from_edge = []
        for param in edge_params:
            if param.name in node_param_names:
                params_from_node.append(param)
            else:
                params_from_edge.append(param)

        frag = frag_template.render(dict(
            source=frag,
            preprocessed=True,
            params_from_node=params_from_node,
            params_from_edge=params_from_edge,
        ))
        p(f'{frag = !s}')

        options['frag'] = f'base64:{b64encode(frag)}'


        options = ',' + ','.join(flatten(options.items()))
        z, x, y = map(str, (z, x, y))
        path = '/'.join((empty, tile, dataset, z, x, y, options))
        url = urlunparse((scheme, netloc, path, params, query, fragment))

        print(url, flush=True)


def cli():
    import argparse
    import sys

    parser = argparse.ArgumentParser()
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
