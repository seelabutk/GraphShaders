"""

"""

import pickle
import csv
from pathlib import Path
import re

from PIL import Image
import numpy as np
import matplotlib as mpl
mpl.use('agg')
import matplotlib.pyplot as plt


def main(output, nodespath, edgespath):
    cache = Path(__file__).with_suffix('.cache.pickle')

    if cache.exists():
        with open(cache, 'rb') as f:
            SC_TC_GotYear_to_Count = pickle.load(f)

    else:
        NodeID_to_GotYear = {}

        with open(nodespath, 'r') as f:
            reader = csv.reader(f)
            assert next(reader) == ['x', 'y', 'appyear', 'gyear', 'nclass', 'cat_ccl']

            for i, row in enumerate(reader, start=1):
                # x = int(row[0], 10)
                # y = int(row[1], 10)
                # appyear = int(row[2], 10)
                gyear = int(row[3], 10)
                # nclass = int(row[4], 10)
                # cat_ccl = int(row[5], 10)

                NodeID_to_GotYear[i] = gyear
        
        SC_TC_GotYear_to_Count = {}

        with open(edgespath, 'r') as f:
            reader = csv.reader(f)
            assert next(reader) == ['citing', 'cited', 'src_cclass', 'tgt_cclass']

            for row in reader:
                citing = int(row[0], 10)
                cited = int(row[1], 10)
                src_cclass = int(float(row[2]))
                tgt_cclass = int(float(row[3]))

                gyear = NodeID_to_GotYear[citing]

                key = (src_cclass, tgt_cclass, gyear)
                SC_TC_GotYear_to_Count[key] = 1 + SC_TC_GotYear_to_Count.get(key, 0)
        
        with open(cache, 'wb') as f:
            pickle.dump(SC_TC_GotYear_to_Count, f)
        
    fig = plt.figure()
    ax = fig.add_subplot(111)
    
    SCs = [2, 2, 5, 5]
    TCs = [2, 5, 2, 5]
    for I in range(len(SCs)):
        SC = SCs[I]
        TC = TCs[I]

        ret = []

        for GotYear in range(1976, 2006+1):
            key = (SC, TC, GotYear)

            ret.append(SC_TC_GotYear_to_Count.get(key, 0))

        ax.plot(ret, label=[
            f'Computers-Computers Citations per Year',
            f'Computers-Mechanical Citations per Year',
            f'Mechanical-Computers Citations per Year',
            f'Mechanical-Mechanical Citations per Year',
        ][I])
    
    ax.legend()

    fig.tight_layout()
    fig.savefig(output)


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--output', '-o', required=True)
    parser.add_argument('--edges', '-e', dest='edgespath', type=Path, required=True)
    parser.add_argument('--nodes', '-n', dest='nodespath', type=Path, required=True)
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
