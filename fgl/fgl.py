"""

"""

from base64 import b64encode, b64decode
from io import StringIO
import itertools
from pathlib import Path
from pprint import pprint
import sys
from urllib.parse import urlparse, urlunparse

from jinja2 import Environment, FileSystemLoader
from pcpp import Preprocessor
from pyglsl_parser import parse
from pyglsl_parser.ast import Ast
from pyglsl_parser.enums import ShaderType, StorageQualifier
from pyglsl_parser.lexemes import Typename


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
    source = preprocess(source)
    ast = parse(source, filename, shader_type=ShaderType.Fragment)

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
    nodeshader = StringIO()
    main_node(nodefile, nodeshader)
    nodeshader = nodeshader.getvalue()

    print(f'{nodeshader = !s}', file=sys.stderr)

    edgeshader = StringIO()
    main_edge(edgefile, edgeshader)
    edgeshader = edgeshader.getvalue()

    print(f'{edgeshader = !s}', file=sys.stderr)

    for url in infile:
        url = url.rstrip()
        scheme, netloc, path, params, query, fragment = urlparse(url)
        empty, tile, dataset, z, x, y, options = path.split('/', 6)
        z, x, y = map(int, (z, x, y))
        options = { k: v for k, v in grouper(options.split(',')[1:], 2) }

        print(f'{b64decode(options["vert"][len("base64:"):]) =!s}', file=sys.stderr)
        print(f'{b64decode(options["frag"][len("base64:"):]) =!s}', file=sys.stderr)

        options['vert'] = f'base64:{b64encode(nodeshader)}'
        options['frag'] = f'base64:{b64encode(edgeshader)}'

        options = ',' + ','.join(flatten(options.items()))
        z, x, y = map(str, (z, x, y))
        path = '/'.join((empty, tile, dataset, z, x, y, options))
        url = urlunparse((scheme, netloc, path, params, query, fragment))

        print(url, file=outfile)


def main_makeurl_repl(infile, outfile):
    for url in infile:
        url = url.rstrip()
        scheme, netloc, path, params, query, fragment = urlparse(url)
        empty, tile, dataset, z, x, y, options = path.split('/', 6)
        z, x, y = map(int, (z, x, y))
        options = { k: v for k, v in grouper(options.split(',')[1:], 2) }

        #print(f'{b64decode(options["vert"][len("base64:"):]) =!s}', file=sys.stderr)
        #print(f'{b64decode(options["frag"][len("base64:"):]) =!s}', file=sys.stderr)
        
        nodefile = StringIO(b64decode(options["vert"][len("base64:"):]))
        nodefile.name = '<url>'
        nodeshader = StringIO()
        try:
            main_node(nodefile, nodeshader)
        except:
            pass
        nodeshader = nodeshader.getvalue()

        edgefile = StringIO(b64decode(options["frag"][len("base64:"):]))
        edgefile.name = '<url>'
        edgeshader = StringIO()
        try:
            main_edge(edgefile, edgeshader)
        except:
            pass
        edgeshader = edgeshader.getvalue()

        options['vert'] = f'base64:{b64encode(nodeshader)}'
        options['frag'] = f'base64:{b64encode(edgeshader)}'

        options = ',' + ','.join(flatten(options.items()))
        z, x, y = map(str, (z, x, y))
        path = '/'.join((empty, tile, dataset, z, x, y, options))
        url = urlunparse((scheme, netloc, path, params, query, fragment))

        print(url, file=outfile, flush=True)


def cli():
    import argparse
    import sys

    parser = argparse.ArgumentParser()
    parser.set_defaults(main=None)
    subparsers = parser.add_subparsers()

    node = subparsers.add_parser('node')
    node.set_defaults(main=main_node)
    node.add_argument('--infile', '-i', default=sys.stdin, type=argparse.FileType('r'))
    node.add_argument('--outfile', '-o', default=sys.stdout, type=argparse.FileType('w'))

    edge = subparsers.add_parser('edge')
    edge.set_defaults(main=main_edge)
    edge.add_argument('--infile', '-i', default=sys.stdin, type=argparse.FileType('r'))
    edge.add_argument('--outfile', '-o', default=sys.stdout, type=argparse.FileType('w'))

    makeurl = subparsers.add_parser('makeurl')
    makeurl.set_defaults(main=main_makeurl)
    makeurl.add_argument('--infile', '-i', default=sys.stdin, type=argparse.FileType('r'))
    makeurl.add_argument('--nodefile', '-n', type=argparse.FileType('r'), required=True)
    makeurl.add_argument('--edgefile', '-e', type=argparse.FileType('r'), required=True)
    makeurl.add_argument('--outfile', '-o', default=sys.stdout, type=argparse.FileType('w'))

    makeurl_repl = subparsers.add_parser('makeurl_repl')
    makeurl_repl.set_defaults(main=main_makeurl_repl)
    makeurl_repl.add_argument('--infile', '-i', default=sys.stdin, type=argparse.FileType('r'))
    makeurl_repl.add_argument('--outfile', '-o', default=sys.stdout, type=argparse.FileType('w'))

    args = vars(parser.parse_args())
    main = args.pop('main')
    main(**args)


if __name__ == '__main__':
    cli()
