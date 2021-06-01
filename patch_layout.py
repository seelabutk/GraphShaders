#! python3
import argparse
import numpy as np
import pandas as pd

from pathlib import Path


def file(s):
    path = Path(s)
    assert path.is_file()
    return path


def dir(s):
    path = Path(s)
    assert path.is_dir()
    return path


def comma_separated_ints(s):
    return list(map(int, s.split(',')))


def edge_columns(s):
    cols = comma_separated_ints(s)
    cols[0] = [cols[0], cols[1]]
    del cols[1]
    return cols


parser = argparse.ArgumentParser()
parser.add_argument('nodesfilename', type=file, help='e.g. data/foo/nodes.csv')
parser.add_argument('edgesfilename', type=file, help='e.g. data/foo/edges.csv')
args = parser.parse_args()

nodes_filename = args.nodesfilename
edges_filename = args.edgesfilename

node_pd = pd.read_csv(nodes_filename)
# Changed for now to patch the gzip version
# edge_pd = pd.read_csv(edges_filename, names=['src', 'tgt'])
edge_pd = pd.read_csv(edges_filename, compression='gzip', sep=' ', names=['src', 'tgt', 'time'])

node_pd.index += 1


elim = edge_pd[edge_pd.src.isin(node_pd.index) & edge_pd.tgt.isin(node_pd.index)]
nodes_in_edges = node_pd[node_pd.index.isin(
    elim.src) | node_pd.index.isin(elim.tgt)]
nodes_in_edges.loc[:, 'new'] = np.arange(1, len(nodes_in_edges)+1)
nodes_in_edges.loc[:, 'old'] = nodes_in_edges.index

mapping = {}
for index, row in nodes_in_edges.iterrows():
    mapping[int(row['old'])] = int(row['new'])

conn_components = edge_pd[edge_pd.src.isin(nodes_in_edges['old']) & edge_pd.tgt.isin(nodes_in_edges['old'])]

count = 0
targ = len(conn_components)
def get_mapping(num): 
    global count
    global targ
    global mapping
    count += 1
    if count % 1000 == 0:
      print('{} out of {}'.format(count, targ))
    return int(mapping[num])

count = 0
def min_max(num, min, max):
    global count
    global targ
    return (num - min) / (max - min)

conn_components.loc[:, 'src'] = conn_components['src'].apply(lambda num: get_mapping(num))
conn_components.loc[:, 'tgt'] = conn_components['tgt'].apply(lambda num: get_mapping(num))
time_min = float(conn_components['time'].min())
time_max = float(conn_components['time'].max())
conn_components.loc[:, 'time'] = conn_components['time'].apply(lambda num: min_max(float(num), time_min, time_max))

del nodes_in_edges['old']
del nodes_in_edges['new']

nodes_in_edges.to_csv('clean_nodes.csv', header=True, index=False)
conn_components.to_csv('clean_edges.csv', header=False, index=False)
