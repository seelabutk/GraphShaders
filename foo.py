#!/usr/bin/env python3.8
"""

"""

from pathlib import Path
import struct
import csv


def main(nodesf, edgesf, root, ncols, ecols, missing):
	nodes = []
	edges = []

	with open(nodesf, 'r') as f:
		f = (x for x in f if x != '')
		reader = csv.reader(f)
		header = next(reader)
		print(header)

		lastrow = None
		for row in reader:
			for c in ncols:
				try:
					float(row[c])
				except ValueError:
					if missing == 'reuse':
						row[c] = lastrow[c]
					else:
						raise
			nodes.append(row)
			lastrow = row

	with open(root / 'nodes.dat', 'wb') as f:
		f.write(struct.pack('Q', len(ncols)))
		for c in ncols:
			f.write(struct.pack('Q', 4 * len(nodes)))
			f.write(struct.pack('Q', ord('f')))
			f.write(struct.pack('Q', len(nodes)))
			lo = min(float(x[c]) for x in nodes)
			hi = max(float(x[c]) for x in nodes)
			f.write(struct.pack('ff', lo, hi))
			for x in nodes:
				f.write(struct.pack('f', float(x[c])))


	with open(edgesf, 'r') as f:
		f = (x for x in f if x != '')
		reader = csv.reader(f)
		header = next(reader)
		for row in reader:
			for c in ecols[0]:
				int(row[c])
                                if row[c] >= len(nodes): continue
			for c in ecols[1:]:
				float(row[c])
			edges.append(row)

	with open(root / 'edges.dat', 'wb') as f:
		f.write(struct.pack('Q', len(ecols)))
		f.write(struct.pack('Q', 2 * 4 * len(edges)))
		f.write(struct.pack('Q', ord('I')))
		f.write(struct.pack('Q', 2 * len(edges)))
		f.write(struct.pack('II', 0, 0))
		for x in edges:
			f.write(struct.pack('II', int(x[ecols[0][0]])-1, int(x[ecols[0][1]])-1))

		for c in ecols[1:]:
			f.write(struct.pack('Q', 4 * len(edges)))
			f.write(struct.pack('Q', ord('f')))
			f.write(struct.pack('Q', len(edges)))
			lo = min(float(x[c]) for x in edges)
			hi = max(float(x[c]) for x in edges)
			f.write(struct.pack('ff', lo, hi))
			for x in edges:
				f.write(struct.pack('f', float(x[c])))


def cli():
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
	
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument('--nodes', dest='nodesf', type=file, required=True, help='e.g. data/foo/nodes.csv')
	parser.add_argument('--edges', dest='edgesf', type=file, required=True, help='e.g. data/foo/edges.csv')
	parser.add_argument('--outdir', dest='root', type=dir, required=True, help='e.g. data/foo')
	parser.add_argument('--ncols', dest='ncols', required=True, type=comma_separated_ints, help='e.g. 0,1,3,4,5,6')
	parser.add_argument('--ecols', dest='ecols', required=True, type=edge_columns, help='e.g. 0,1')
	parser.add_argument('--missing', dest='missing', choices=('error', 'reuse'), required=True, help='e.g. "error"')
	args = vars(parser.parse_args())

	main(**args)


if __name__ == '__main__':
	cli()

