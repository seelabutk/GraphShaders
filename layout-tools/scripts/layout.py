#!/usr/bin/env python3

import graph_tool
from graph_tool import draw
import sys
import numpy as np
import gzip
import csv

filename = sys.argv[1]


def take(iterable, n):
    for i, x in enumerate(iterable):
        if i == n:
            break
        yield x


'''
with gzip.open(filename,  'rt', errors='replace', encoding='ascii') as f:
    f = (x.replace('\x00', '?') for x in f)
    g = graph_tool.load_graph_from_csv(
        f, directed=False, csv_options=dict(delimiter=",",
                                            quoting=csv.QUOTE_NONE))
'''
g = graph_tool.load_graph_from_csv(filename, directed=False, csv_options=dict(
    delimiter=',', quoting=csv.QUOTE_NONE), skip_first=True)
while True:
    layout = draw.arf_layout(g, max_iter=1000, d=0.8, dt=1e-6)
    g.reindex_edges()

    print('Writing...')
    with open(filename + '.vert', 'wt', encoding='ascii', errors='replace') as fp:
        positions = layout.get_2d_array([0, 1]).T.tolist()
        for i, pos in enumerate(positions):
            fp.write('%f,%f,%s\n' %
                     (pos[0], pos[1], g.vp['name'][g.vertex(i)]))

    with open(filename + '.edge', 'w') as fp:
        for i in g.edges():
            source = g.vertex_index[i.source()]
            target = g.vertex_index[i.target()]
            fp.write('%d,%d\n' % (source, target))
    print('Done Writing...')
