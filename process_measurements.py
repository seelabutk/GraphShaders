"""

"""

from __future__ import annotations
import csv
import json
from pathlib import Path


def main(infile, outfile):
    writer = csv.writer(outfile)
    writer.writerow(['elapsed', 'status', 'dt'])

    for line in infile:
        if line == '':
            continue

        data = json.loads(line)
        for row in data:
            if row['status'] == 'active':
                continue

            writer.writerow([row['elapsed'], row['status'], row['dt']])


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--input', dest='infile', type=argparse.FileType('r'), default=str(Path(__file__).parent / 'measurements.json'))
    parser.add_argument('--output', dest='outfile', type=argparse.FileType('w'), default=str(Path(__file__).parent / 'measurements.csv'))
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
