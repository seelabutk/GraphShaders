"""

"""

import re

from PIL import Image
import numpy as np
import matplotlib as mpl
mpl.use('agg')
import matplotlib.pyplot as plt


def guess(filename):
    match = re.search(r'I=(?P<I>\d+)', filename)
    if not match:
        raise ValueError(f'Expected to find I= within {filename = }')
    
    I = match.group('I')
    return int(I, 10)


def main(output, filenames):
    I = guess(filenames[0])
    N = len(filenames)
    BINS = 32
    AXIS = 0

    hists = []

    for i, filename in enumerate(filenames):
        image = Image.open(filename)
        data = np.array(image)
        data = data[:, :, 0]
        print(data[:5, :5])
        hist = np.sum(np.sum(data != 255, axis=AXIS).reshape((-1, BINS)), axis=1)

        hists.append(hist)

    hists = np.array(hists)
    print(f'{hists.shape = }')
    print(hists)

    fig = plt.figure()
    ax = fig.add_subplot(111)

    im = ax.imshow(hists)
    fig.colorbar(im)

    ax.set_title([
        f'Computers-Computers Citations per Year',
        f'Computers-Mechanical Citations per Year',
        f'Mechanical-Computers Citations per Year',
        f'Mechanical-Mechanical Citations per Year',
    ][I])

    ax.set_xticks(np.arange(BINS))
    ax.set_xticklabels([f'{j}' for j in range(BINS)], rotation=90)
    ax.set_xlabel(f'Image Row ({BINS} bins)' if AXIS == 1 else f'Image Column ({BINS} bins)')

    ax.set_yticks(np.arange(N))
    ax.set_yticklabels([f'{1976+j}' for j in range(N)])
    ax.set_ylabel(f'Year that Patent was Approved')

    fig.tight_layout()
    fig.savefig(output)


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--output', '-o', required=True)
    parser.add_argument('filenames', nargs='+')
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
