#!/usr/bin/env python3.8

from pathlib import Path
import struct
import csv

root = Path('data.bak/knit-graph')
nodesf = root / 'nodes.csv'
edgesf = root / 'edges.csv'

nodes = []
edges = []

with open(nodesf, 'r') as f:
	f = (x for x in f if x != '')
	reader = csv.reader(f)
	header = next(reader)
	print(header)

	lastthird = None
	for row in reader:
		float(row[0])
		float(row[1])
		try:
			float(row[3])
		except ValueError:
			row[3] = lastthird

		try:
			float(row[4])
		except ValueError:
			row[4] = '0'
		float(row[5])
		#float(row[6])
		nodes.append(row)
		lastthird = row[3]

with open(root / 'nodes.dat', 'wb') as f:
	f.write(struct.pack('Q', 5))
	for c in (0, 1, 3, 4, 5):
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
		edges.append(row)

with open(root / 'edges.dat', 'wb') as f:
	f.write(struct.pack('Q', 1))
	
	f.write(struct.pack('Q', 2 * 4 * len(edges)))
	f.write(struct.pack('Q', ord('I')))
	f.write(struct.pack('Q', 2 * len(edges)))
	f.write(struct.pack('II', 0, 0))
	for x in edges:
		f.write(struct.pack('II', int(x[0]) - 1, int(x[1]) - 1))
