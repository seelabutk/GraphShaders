"""

"""

import re

from PIL import Image
import numpy as np
import matplotlib as mpl
mpl.use('agg')
import matplotlib.pyplot as plt


def guess_I(filename):
    match = re.search(r'I=(?P<I>\d+)', filename)
    if not match:
        raise ValueError(f'Expected to find I= within {filename = }')
    
    I = match.group('I')
    return int(I, 10)


def guess_J(filename):
    match = re.search(r'J=(?P<J>\d+)', filename)
    if not match:
        raise ValueError(f'Expected to find I= within {filename = }')
    
    J = match.group('J')
    return int(J, 10)


def main(output, filenames):
    N = len(filenames)
    II = set()
    JJ = set()
    for filename in filenames:
        I = guess_I(filename)
        J = guess_J(filename)

        II.add(I)
        JJ.add(J)
    
    II = sorted(II)
    JJ = sorted(JJ)
    
    I2index = { x: i for i, x in enumerate(II) }
    J2index = { x: i for i, x in enumerate(JJ) }
    ret = np.zeros((len(I2index), len(J2index)), dtype=float)

    for filename in filenames:
        I = guess_I(filename)
        J = guess_J(filename)
        image = Image.open(filename)
        data = np.array(image)
        data = data[:, :, 0]
        data = 100 * np.sum(data != 255.) / np.prod(data.shape)

        ret[I2index[I], J2index[J]] = data

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for I in II:
        ax.plot(ret[I2index[I], :], label=[
            f'Computers-Computers Citations per Year',
            f'Computers-Mechanical Citations per Year',
            f'Mechanical-Computers Citations per Year',
            f'Mechanical-Mechanical Citations per Year',
        ][I])
    
    ax.legend()

    ax.set_title(f'Percentage of Non-White Pixels for Citation Categories and Years')

    ax.set_xticks(JJ)
    ax.set_xticklabels([f'{1976+J}' for J in JJ], rotation=90)
    ax.set_xlabel(f'Year that Patent was Approved')

    ax.set_ylabel(f'Percentage of Total Pixels that are Non-White')

    # ax.set_xticks(np.arange(BINS))
    # ax.set_xticklabels([f'{j}' for j in range(BINS)], rotation=90)
    # ax.set_xlabel(f'Image Row ({BINS} bins)' if AXIS == 1 else f'Image Column ({BINS} bins)')

    # ax.set_yticks(np.arange(N))
    # ax.set_yticklabels([f'{1976+j}' for j in range(N)])
    # ax.set_ylabel(f'Year that Patent was Approved')

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
