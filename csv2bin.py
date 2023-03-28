"""

"""

from __future__ import annotations
from pathlib import Path
import csv
import struct
import itertools


TRANSFORMS = {
    'float': float,
    'int': int,
    '-1': lambda x: x-1,
}


def main(
    *,
    src: Path,
    dst: Path,
    cols: List[str],
    transforms: List[str],
    format: str,
    domain: Optional[Tuple[float, float]],
):
    # print(f'{src = !r}')
    # print(f'{dst = !r}')
    # print(f'{cols = !r}')
    # print(f'{transforms = !r}')
    # print(f'{format = !r}')

    # Verify valid format
    _ = struct.calcsize(format)

    if domain is not None:
        print(f'{domain = !r}')
        TRANSFORMS['normalize'] = lambda x: (x - domain[0]) / (domain[1] - domain[0])

    transforms = tuple(TRANSFORMS[transform] for transform in transforms)

    with open(src, 'rt') as src:
        reader = csv.reader(src)

        header = next(reader)
        header = { k: i for i, k in enumerate(header) }

        cols = tuple(header[col] for col in cols)

        minimum = None
        maximum = None

        with open(dst, 'wb') as dst:
            for row in reader:
                row = tuple(row[col] for col in cols)

                for transform in transforms:
                    row = tuple(transform(x) for x in row)
                
                if minimum is None:
                    minimum = row

                if maximum is None:
                    maximum = row
                
                minimum = tuple(itertools.starmap(min, zip(minimum, row)))
                maximum = tuple(itertools.starmap(max, zip(minimum, row)))

                dst.write(struct.pack(format, *row))
            
            print(f'Wrote {dst.tell()} bytes to {dst.name}')
    
    print(f'Domain: [{minimum}, {maximum}]')
    if len(minimum) == 1:
        print(f'  --domain {minimum[0]},{maximum[0]}')


def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--input', '-i', dest='src', type=Path, required=True)
    parser.add_argument('--output', '-o', dest='dst', type=Path, required=True)
    parser.add_argument('--column', '-c', dest='cols', default=[], action='append', required=True)
    parser.add_argument('--transform', '-t', dest='transforms', default=[], action='append', required=True)
    parser.add_argument('--format', '-f', dest='format', required=True)
    parser.add_argument('--domain', '-n', dest='domain', type=lambda x: tuple(map(float, x.split(','))), required=False)
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
