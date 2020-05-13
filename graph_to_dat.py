#!/usr/bin/env python3.7
"""

"""

import struct
import csv
from pathlib import Path


def main(graph_root):
	with open(graph_root / 'nodes.csv', 'r') as f:
		print(f.readline())


def cli():
	def graph_root(s):
		root = Path(s)
		if not root.is_dir():
			raise argparse.ArgumentTypeError(f'{root} not a directory')
		nodes = root / 'nodes.csv'
		if not nodes.is_file():
			raise argparse.ArgumentTypeError(f'{nodes} not a file')
		edges = root / 'edges.csv'
		if not edges.is_file():
			raise argparse.ArgumentTypeError(f'{edges} not a file')
		return root

	import argparse
	
	parser = argparse.ArgumentParser()
	parser.add_argument('graph_root', type=graph_root)
	args = vars(parser.parse_args())

	main(**args)


if __name__ == '__main__':
	cli()
