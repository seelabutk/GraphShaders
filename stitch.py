"""

"""

from concurrent.futures import ThreadPoolExecutor
from io import BytesIO
from threading import Lock
from urllib.parse import urlparse, urlunparse

from PIL import Image
import requests


def main(infile, outfile, z_inc, nthreads, mode):
    for url in infile:
        url = url.rstrip()
        scheme, netloc, path, params, query, fragment = urlparse(url)
        empty, tile, dataset, z, x, y, options = path.split('/', 6)
        z, x, y = map(int, (z, x, y))

        z0 = z
        z1 = z + z_inc

        x0, y0 = x, y
        x0, y0 = 2**z_inc * x0, 2**z_inc * y0

        x1, y1 = x+2, y+2
        x1, y1 = 2**z_inc * x1, 2**z_inc * y1

        tilesize = 256
        width = 2**(z_inc+1) * tilesize
        height = 2**(z_inc+1) * tilesize

        lock = Lock()
        if mode == 'stitch':
            canvas = Image.new('RGB', (width, height))
        elif mode == 'noop':
            pass

        count = 0

        def worker(i, j):
            nonlocal count

            with lock:
                count += 1
                mycount = count
                
                z = z1
                x = j
                y = i
                # print(f'Requesting image {mycount}/{(2**(z_inc+1))**2} ({z=} {x=} {y=})')

            z, x, y = map(str, (z, x, y))
            path = '/'.join((empty, tile, dataset, z, x, y, options))
            url = urlunparse((scheme, netloc, path, params, query, fragment))
            # print(f'{url = !s}')

            with requests.get(url, headers={'Connection': 'close'}) as r:
                if mode == 'stitch':
                    image = Image.open(BytesIO(r.content))
                elif mode == 'noop':
                    _ = r.content

            with lock:
                # print(f'Stitching image {mycount}/{(2**(z_inc+1))**2} ({z=} {x=} {y=})')
                if mode == 'stitch':
                    canvas.paste(image, ((j-x0) * tilesize, (i-y0) * tilesize))
                elif mode == 'noop':
                    pass

        with ThreadPoolExecutor(max_workers=nthreads) as executor:
            for i in range(y0, y1):
                for j in range(x0, x1):
                    executor.submit(worker, i, j)

        if mode == 'stitch':
            canvas.save(outfile)
        elif mode == 'noop':
            pass

def cli():
    import argparse
    import sys

    parser = argparse.ArgumentParser()
    parser.add_argument('--infile', '-i', default=sys.stdin, type=argparse.FileType('r'))
    parser.add_argument('--z-inc', '-z', default=2, type=int)
    parser.add_argument('--outfile', '-o', required=True, type=argparse.FileType('wb'))
    parser.add_argument('--nthreads', '-n', default=None, type=int)
    parser.add_argument('--mode', '-m', choices=('stitch', 'noop'), default='stitch')
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
