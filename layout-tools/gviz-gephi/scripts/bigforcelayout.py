import argparse
import igraph
import os
import subprocess

from datetime import datetime


# This should be illegal, I digress
def convertToFMT(graph: igraph.Graph, fmt: str, infile: str) -> str:
    valid = ['.lgl', '.dot', '.ncol']
    assert(fmt in valid)
    tail = os.path.basename(infile)
    tmpname = '{}/{}{}'.format(tmpdir, tail, fmt)
    print('Saving tmp file type to {}'.format(tmpname))
    if fmt == '.lgl':
        graph.write_lgl(tmpname)
    elif fmt == '.dot':
        graph.write_dot(tmpname)
    elif tm == '.ncol':
        graph.write_ncol(tmpname)
    return tmpname


parser = argparse.ArgumentParser()

parser.add_argument('--input', help='input file', dest='input',
                    action='store', required=True)
parser.add_argument('--output', help='output dir', dest='output',
                    action='store', required=True)
parser.add_argument('--outtype', help='output file type',
                    dest='outtype', choices=['edge', 'pickle', 'ncol'],
                    action='store', required=True)
parser.add_argument(
    '--iter', help='Iterations to perform', dest='iter',
    action='store', default=100)
parser.add_argument('--layout', help='layout algorithm to use',
                    choices=['lgl', 'opt', 'drl'], dest='layout',
                    action='store', required=True)
parser.add_argument('--indef', '-n', help='Run indifinitely?',
                    dest='indef', action='store_true', default=False)
parser.add_argument('--tmpdir', help='Tmp save directory', dest='tmpdir',
                    action='store', default='/bfl-tmp')

parsed = parser.parse_args()

infile = parsed.input
output = parsed.output
iters = int(parsed.iter)
isindef = parsed.indef
omat = parsed.outtype
layout = parsed.layout
tmpdir = parsed.tmpdir

dirname = os.path.dirname(output)
if not os.path.isdir(dirname):
    print("{} is not a valid directory to output to!".format(dirname))
    exit(1)

csvcheck = infile[-4:]
if csvcheck == ".csv":
    print('.csv must be in space delimitted format: NUM NUM')
    print('Verifying format, saving to tmp directory if not.')
    if not os.path.isdir(tmpdir):
        os.makedirs(tmpdir, 777, exist_ok=True)
    targ = open(infile, 'r')
    tail = os.path.basename(infile)
    tmpname = '{}/{}.tmp.edge'.format(tmpdir, tail)
    tmpfile = open(tmpname, 'w')

    for line in targ:
        split = line.strip()
        split = split.split(',')
        if len(split) > 2 or not split[0].isdigit() or not split[1].isdigit():
            print('skipping: {}'.format(line))
            continue
        print('{} {}'.format(split[0], split[1]), file=tmpfile)
    targ.close()
    tmpfile.close()
    infile = tmpname

graph = igraph.load(infile)
graph.to_undirected(mode='each')
coords = igraph.Layout()

cont = True
while cont:
    outfile = open(output, 'w')
    print('Laying Out...')
    if layout == 'lgl':
        # coords = graph.layout_lgl(maxiter=iters)
        print('Converting to LGL file...')
        workfile = convertToFMT(graph, '.lgl', infile)
        tmpfile = './lgl.out'
        print('Starting lgl...')
        subprocess.call(['lglayout2D', workfile, '-t', '1000', tmpfile])
        print('LGL layout complete, converting to Fast Graph format')
        subprocess.call(['sort', '-n', '-k', '1', '-o', tmpfile, tmpfile])
        subprocess.call(['sed', '-i', 's/1e+06/000.000/g', '-i', tmpfile])
        repl1 = 's/^[0-9]* (-*[0-9eE]*\\.[0-9eE]*)'
        repl2 = '(-*[0-9eE]*\\.[0-9eE]*)/\\1,\\2/g'
        repl = '{} {}'.format(repl1, repl2)
        subprocess.call(['sed', '-E', '-i', repl, tmpfile])
        subprocess.call(['mv', tmpfile, output])
    elif layout == 'opt':
        coords = graph.layout_graphopt(niter=iters)
        print(coords)
        print('Writing...')
        for coord in coords:
            x = coord[0]
            y = coord[1]
            print('{},{}'.format(x, y), file=outfile)
    elif layout == 'drl':
        coords = graph.layout_drl()
        print(coords)
        print('Writing...')
        for coord in coords:
            x = coord[0]
            y = coord[1]
            print('{},{}'.format(x, y), file=outfile)

    cont = isindef
    outfile.close()
