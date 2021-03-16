"""

"""

from __future__ import annotations
from contextlib import contextmanager
import csv
from dataclasses import dataclass
from datetime import datetime, timedelta
from io import TextIOWrapper
from pathlib import Path
import re
from statistics import mean, stdev
import subprocess
import sys
from typing import *

from typing_extensions import get_type_hints, get_origin, get_args, Annotated


@contextmanager
def subprocess_as_stdin(args):
    process = subprocess.Popen(
        args=args,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )

    old_stdin = sys.stdin
    sys.stdin = process.stdout

    try:
        yield
    finally:
        process.stdout.close()
        process.terminate()
        process.wait(timeout=1)
        process.kill()
        process.wait()

        sys.stdin = old_stdin



@dataclass
class TreeValueProxy:
    tree: Tree

    def __getitem__(self, key) -> str:
        ret = self.tree[key]
        if ret is None:
            return None
        return ret[1]

    @property
    def logs(self) -> Dict[str, str]:
        return { k: v[1] for k, v in self.tree.logs.items() }

    @property
    def attributes(self) -> Dict[str, datetime]:
        return { k: v[1] for k, v in self.tree.attributes.items() }



@dataclass
class TreeTimestampProxy:
    tree: Tree

    def __getitem__(self, key) -> datetime:
        ret = self.tree[key]
        if ret is None:
            return None
        return ret[0]

    @property
    def logs(self) -> Dict[str, datetime]:
        return { k: v[0] for k, v in self.tree.logs.items() }

    @property
    def attributes(self) -> Dict[str, datetime]:
        return { k: v[0] for k, v in self.tree.attributes.items() }


class TreeChildrenProxy(list):
    def match(self, pattern) -> Iterator[Tree]:
        for tree in self:
            yield from tree.match(pattern)


@dataclass
class Tree:
    attributes: Dict[str, Tuple[datetime, str]]
    logs: Dict[str, Tuple[datetime, str]]
    _children: List[Tree]
    parent: Tree

    @property
    def values(self) -> TreeValueProxy:
        return TreeValueProxy(self)

    @property
    def times(self) -> TreeTimestampProxy:
        return TreeTimestampProxy(self)

    @property
    def children(self) -> TreeChildrenProxy:
        return TreeChildrenProxy(self._children)

    @property
    def duration(self) -> timedelta:
        return self.times['finished'] - self.times['started']

    def __getitem__(self, key) -> Tuple[datetime, str]:
        if (value := self.attributes.get(f'@{key}', None)) is not None:
            return value
        if (value := self.logs.get(key, None)) is not None:
            return value
        if key == 'children':
            return self.children
        return None

    RE = re.compile(r'''
        ^
        (?P<task_id>[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-z0-9]{12})  # e.g. "6a3ad23f-b0c1-47dd-9913-cf537e950839"
        /(?P<task_level>[0-9]+(?:/[0-9]+)*)                                        # e.g. "/8/5/17/5/4"
        [\t]
        (?P<timestamp>[0-9]+\.[0-9]*)                                              # e.g. "1613403872.549264"
        [\t]
        (?P<variable>[^\t]+)                                                       # e.g. "@finished"
        [\t]?
        (?P<value>.*)
        $
    ''', re.VERBOSE)

    @classmethod
    def readiter(cls, infiles: List[Path], *, presorted: bool=False) -> Iterator[Tree]:
        if not presorted:
            args = [
                'sort',
                '-t\t',      # tab delimited
                '-k1,1.36',  # sort by first column, first 36 characters (task id)
                '-k1.36V',   # sort by first column, characters after first 36, as a version (task levels)
                '-k2,2n',    # sort by second column, as a number (timestamp)
                *infiles,    # sort these files
            ]
        else:
            args = [
                'sort',
                '-t\t',      # tab delimited
                '-k1,1.36',  # sort by first column, first 36 characters (task id)
                '-k1.36V',   # sort by first column, characters after first 36, as a version (task levels)
                '-k2,2n',    # sort by second column, as a number (timestamp)
                '-m',        # merge already sorted files
                *infiles,    # sort these files
            ]

        with subprocess_as_stdin(args):
            last_task_id = None
            last_task_level = None
            last_timestamp = None
            last_variable = None
            last_value = None

            root = None
            tree = None

            for line in sys.stdin:
                line = line.rstrip()
                match = cls.RE.match(line)
                if not match:
                    print( ValueError(f'{line=} does not match {cls.RE=}') )
                    continue

                task_id = match.group('task_id')
                task_level = tuple(map(int, match.group('task_level').split('/')))
                timestamp = datetime.fromtimestamp(float(match.group('timestamp')))
                variable = match.group('variable')
                value = match.group('value')

                #print(f'{task_id=} {task_level=} {timestamp=} {variable=} {value=}')

                if last_task_id is not None and task_id != last_task_id:
                    yield root
                    root = None
                    tree = None

                if tree is None:
                    tree = cls({}, {}, [], None)
                    last_task_id = None
                    last_task_level = None
                    last_timestamp = None
                    last_variable = None
                    last_value = None

                if root is None:
                    root = tree

                if last_task_id is not None and len(task_level) > len(last_task_level):
                    tree = cls({}, {}, [], tree)
                    tree.parent._children.append(tree)

                if last_task_id is not None and len(task_level) < len(last_task_level):
                    tree = tree.parent

                if last_task_id is not None and len(task_level) == len(last_task_level):
                    if task_level[-1] < last_task_level[-1]:
                        tree = cls({}, {}, [], tree.parent)
                        tree.parent._children.append(tree)

                if variable.startswith('@'):
                    tree.attributes[variable] = (timestamp, value)
                else:
                    tree.logs[variable] = (timestamp, value)

                last_task_id = task_id
                last_task_level = task_level
                last_timestamp = timestamp
                last_variable = variable
                last_value = value

        if root is not None:
            yield root

    def pprint(self):
        def p(tree, depth=0):
            for attr, (timestamp, value) in tree.attributes.items():
                print(f'{" "*depth}{timestamp} {attr}={value!r}')
            for log, (timestamp, value) in tree.logs.items():
                print(f'{" "*depth}{timestamp} {log}={value!r}')
            for tree in tree._children:
                p(tree, depth+2)

        p(self)

    def match(self, pattern):
        class _Temporary:
            p: pattern

        parsed_pattern = get_type_hints(_Temporary, localns={'pattern': pattern}, include_extras=True)['p']
        if get_origin(parsed_pattern) is Annotated:
            parsed_pattern, recursive = get_args(parsed_pattern)
        else:
            recursive = False

        annotations = get_type_hints(parsed_pattern, include_extras=True)
        for name, expected in annotations.items():
            #print(f'{expected=} {self[name]=}')
            expected_any = expected is Any
            got_something = self[name] is not None
            if expected_any and not got_something:
                break

            expected_tuple = get_origin(expected) is tuple
            got_something = self[name] is not None
            if expected_tuple and not got_something:
                break
            elif expected_tuple and got_something:
                exp_timestamp, exp_value = get_args(expected)
                got_timestamp, got_value = self[name]

                #print(f'{exp_timestamp=} {exp_value=}')
                #print(f'{got_timestamp=} {got_value=}')

                expected_any = exp_timestamp is Any
                got_something = got_timestamp is not None
                if expected_any and not got_something:
                    break

                expected_any = exp_value is Any
                got_something = got_value is not None
                if expected_any and not got_something:
                    break

                if get_origin(exp_value) is Literal:
                    exp_value = get_args(exp_value)[0]
                    if exp_value != got_value:
                        break

            expected_list = get_origin(expected) is list
            got_something = self[name] is not None
            if expected_list and not got_something:
                break
            elif expected_list and got_something:
                exp_class = get_args(expected)[0]

                for tree in self._children:
                    if next(tree.match(exp_class), None) is not None:
                        break

        else:
            yield self
            return

        if recursive:
            for tree in self._children:
                yield from tree.match(pattern)


T = TypeVar('T')
Recursive = Annotated[T, True]
NonRecursive = Annotated[T, False]


class ClientRequest:
    started: Tuple[Any, Literal['requesting tile']]
    finished: Tuple[Any, Any]


class ServerResponse:
    started: Tuple[Any, Literal['answer tile request']]
    finished: Tuple[Any, Literal['']]
    #error: Optional[Any]
    rxsize: Any
    txsize: Any
    x: Any
    y: Any
    z: Any
    dataset: Any
    opt_dcIdent: Any
    

class SendToRenderThread:
    started: Tuple[Any, Literal['send to render thread']]
    finished: Tuple[Any, Literal['']]


class Lock:
    started: Tuple[Any, Literal['lock']]
    finished: Tuple[Any, Literal['']]


class ReceiveFromRequestThread:
    started: Tuple[Any, Literal['receive from request thread']]
    finished: Tuple[Any, Literal['']]


class Render:
    started: Tuple[Any, Literal['render']]
    finished: Tuple[Any, Literal['']]


class RenderingTiles:
    started: Tuple[Any, Literal['rendering tiles']]
    finished: Tuple[Any, Literal['']]
    top: Any
    left: Any
    numTilesX: Any
    numTilesY: Any
    zoom: Any


class JpegEncoding:
    started: Tuple[Any, Literal['jpeg']]
    finished: Tuple[Any, Literal['']]
    resolution: Any


class Unlock:
    started: Tuple[Any, Literal['unlock']]
    finished: Tuple[Any, Literal['']]


def main_metrics(outfile, infiles, presorted):
    STR = '%s'
    INT = '%d'
    FLOAT = '%f'
    fieldformats = {
        'dataset': STR,
        'z': FLOAT, 'x': FLOAT, 'y': FLOAT, 'opt_dcIdent': INT, 'resolution': INT,
        'partition_left': INT, 'partition_top': INT, 'partition_width': INT, 'partition_height': INT,
        'rxsize': INT, 'txsize': INT,
        'client_latency': FLOAT, 'server_latency': FLOAT, 'lock_latency': FLOAT, 'unlock_latency': FLOAT, 'jpeg_latency': FLOAT, 'prepare_latency': FLOAT, 'render_latency': FLOAT,
        'error': STR,
    }
    writer = csv.DictWriter(outfile, list(fieldformats.keys()))
    writer.writeheader()

    for tree in Tree.readiter(infiles, presorted=presorted):
        row = { k: None for k in fieldformats }
        for tree in tree.match(ClientRequest):
            row['client_latency'] = tree.duration.total_seconds()

            for tree in tree.children.match(ServerResponse):
                row['rxsize'] = int(tree.values['rxsize'])
                row['txsize'] = int(tree.values['txsize'])
                row['x'] = float(tree.values['x'])
                row['y'] = float(tree.values['y'])
                row['z'] = float(tree.values['z'])
                row['opt_dcIdent'] = int(tree.values['opt_dcIdent'])
                row['dataset'] = tree.values['dataset']
                row['error'] = tree.values['error']
                row['server_latency'] = tree.duration.total_seconds()

                for tree in tree.children.match(SendToRenderThread):
                    for lock in tree.children.match(Lock):
                        row['lock_latency'] = lock.duration.total_seconds()

                    for unlock in tree.children.match(Unlock):
                        row['unlock_latency'] = unlock.duration.total_seconds()

                    for jpeg in tree.children.match(JpegEncoding):
                        row['resolution'] = int(jpeg.values['resolution'])
                        row['jpeg_latency'] = jpeg.duration.total_seconds()

                    for tree in tree.children.match(ReceiveFromRequestThread):
                        row['prepare_latency'] = tree.duration.total_seconds()

                        for tree in tree.children.match(Render):
                            row['render_latency'] = tree.duration.total_seconds()

                            for tree in tree.children.match(RenderingTiles):
                                row['partition_left'] = int(tree.values['left'])
                                row['partition_top'] = int(tree.values['top'])
                                row['partition_width'] = int(tree.values['numTilesX'])
                                row['partition_height'] = int(tree.values['numTilesY'])

        row = { k: fieldformats[k] % (v,) if v is not None else None for k, v in row.items() }
        writer.writerow(row)


def main_debug(infiles, presorted):
    for tree in Tree.readiter(infiles, presorted=presorted):
        tree.pprint()
        print()


def cli():
    import argparse

    parser = argparse.ArgumentParser()

    parser.set_defaults(main=None)
    subparsers = parser.add_subparsers(required=True)

    metrics = subparsers.add_parser('metrics')
    metrics.set_defaults(main=main_metrics)
    metrics.add_argument('--outfile', '-o', default=sys.stdout, type=argparse.FileType('w'))
    metrics.add_argument('--presorted', action='store_true')
    metrics.add_argument('infiles', nargs='+', type=Path)

    debug = subparsers.add_parser('debug')
    debug.set_defaults(main=main_debug)
    debug.add_argument('--presorted', action='store_true')
    debug.add_argument('infiles', nargs='+', type=Path)

    args = vars(parser.parse_args())
    main = args.pop('main')
    main(**args)


if __name__ == '__main__':
    cli()
