from __future__ import annotations
from contextlib import contextmanager
import csv
from dataclasses import dataclass
from datetime import datetime
from io import TextIOWrapper
from pathlib import Path
import re
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
        return self.tree[key][1]

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
        return self.tree[key][0]

    @property
    def logs(self) -> Dict[str, datetime]:
        return { k: v[0] for k, v in self.tree.logs.items() }

    @property
    def attributes(self) -> Dict[str, datetime]:
        return { k: v[0] for k, v in self.tree.attributes.items() }


@dataclass
class Tree:
    attributes: Dict[str, Tuple[datetime, str]]
    logs: Dict[str, Tuple[datetime, str]]
    children: List[Tree]
    parent: Tree

    @property
    def values(self) -> TreeValueProxy:
        return TreeValueProxy(self)

    @property
    def times(self) -> TreeTimestampProxy:
        return TreeTimestampProxy(self)


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
    def readiter(cls, infiles: List[Path]) -> Iterator[Tree]:
        with subprocess_as_stdin([
            'sort',
            '-t\t',      # tab delimited
            '-k1,1.36',  # sort by first column, first 36 characters (task id)
            '-k1.36V',   # sort by first column, characters after first 36, as a version (task levels)
            '-k2,2n',    # sort by second column, as a number (timestamp)
            *infiles,    # sort these files
        ]):
            last_task_id = None
            last_task_level = None
            last_timestamp = None
            last_variable = None
            last_value = None

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
                    yield tree
                    tree = None

                if tree is None:
                    tree = cls({}, {}, [], None)
                    last_task_id = None
                    last_task_level = None
                    last_timestamp = None
                    last_variable = None
                    last_value = None
                    #print('new tree')

                if last_task_id is not None and len(task_level) > len(last_task_level):
                    tree = cls({}, {}, [], tree)
                    tree.parent.children.append(tree)

                if last_task_id is not None and len(task_level) < len(last_task_level):
                    tree = tree.parent

                if last_task_id is not None and len(task_level) == len(last_task_level):
                    if task_level[-1] < last_task_level[-1]:
                        tree = cls({}, {}, [], tree.parent)
                        tree.parent.children.append(tree)

                if variable.startswith('@'):
                    tree.attributes[variable] = (timestamp, value)
                else:
                    tree.logs[variable] = (timestamp, value)

                last_task_id = task_id
                last_task_level = task_level
                last_timestamp = timestamp
                last_variable = variable
                last_value = value

        if tree is not None:
            yield tree
    
    def pprint(self):
        def p(tree, depth=0):
            for attr, (timestamp, value) in tree.attributes.items():
                print(f'{" "*depth}{timestamp} {attr}={value!r}')
            for log, (timestamp, value) in tree.logs.items():
                print(f'{" "*depth}{timestamp} {log}={value!r}')
            for tree in tree.children:
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

                for tree in self.children:
                    if next(tree.match(exp_class), None) is not None:
                        break

        else:
            yield self
            return

        if recursive:
            for tree in self.children:
                yield from tree.match(pattern)



T = TypeVar('T')
Recursive = Annotated[T, True]
NonRecursive = Annotated[T, False]


class BatchPattern:
    started: Tuple[Any, Literal['batch']]
    finished: Tuple[Any, Literal['batch']]
    epoch: Any
    loss: Any
    accuracy: Any


class EpochPattern:
    started: Tuple[Any, Literal['epoch']]
    finished: Tuple[Any, Literal['epoch']]
    epoch: Any
    loss: Any
    accuracy: Any
    children: List[BatchPattern]


class TrainPattern:
    started: Tuple[Any, Literal['epoch']]
    finished: Tuple[Any, Literal['epoch']]
    children: List[Recursive[BatchPattern]]


class TrialPattern:
    started: Tuple[Any, Literal['trial']]
    finished: Tuple[Any, Literal['trial']]
    children: List[Recursive[TrainPattern]]


class TripleRPattern:
    started: Tuple[Any, Literal['triple-r.py']]
    finished: Tuple[Any, Literal['triple-r.py']]
    


def main(outfile, infiles):
    writer = csv.writer(outfile)

    '''for tree in Tree.readiter(infiles):
        Epoch = NewType('Epoch', int)
        Batch = NewType('Batch', int)
        Seconds = NewType('Seconds', float)
        Loss = NewType('Loss', float)
        Accuracy = NewType('Accuracy', float)

        per_epoch: Dict[Tuple[Epoch], Tuple[Seconds, Loss, Accuracy]] = {}
        per_batch: Dict[Tuple[Epoch, Batch], Tuple[Seconds, Loss]] = {}
        parameters = {}

        match = next(tree.match(Recursive[TripleRPattern]), None)
        if match is not None:
            parameters['events'] = match.values['events']

        for trial in tree.match(Recursive[TripleRPattern]):
            total_trial_time = (trial.times['finished'] - trial.times['started']).total_seconds()
            print(f'{total_trial_time = }s')
            parameters = { **parameters, **trial.values.logs }
            for train in tree.match(Recursive[TrainPattern]):
                for epoch in train.match(Recursive[EpochPattern]):
                    per_epoch[(int(epoch.values['epoch']),)] = (
                        (epoch.times['finished'] - epoch.times['started']).total_seconds(),
                        float(epoch.values['loss']),
                        float(epoch.values['accuracy']),
                    )

                    for batch in epoch.children:
                        per_batch[int(epoch.values['epoch']), int(batch.values['batch'])] = (
                            (batch.times['finished'] - batch.times['started']).total_seconds(),
                            float(batch.values['loss']),
                        )

        print(parameters)

        writer.writerow(['=== EPOCH ==='])
        writer.writerow(['epoch', 'seconds', 'loss', 'accuracy'])
        for key, row in per_epoch.items():
            writer.writerow([*key, *row])

        writer.writerow(['=== BATCH ==='])
        writer.writerow(['epoch', 'batch', 'seconds', 'loss'])
        for key, row in per_batch.items():
            writer.writerow([*key, *row])'''

    class RenderPattern:
        started: Tuple[Any, Literal['answer tile request']]
        finished: Tuple[Any, Literal['']]
    
    for tree in Tree.readiter(infiles):
        for render in tree.match(Recursive[RenderPattern]):
            start = render.times['started']
            finish = render.times['finished']
            print(f'{(finish - start).total_seconds()}')

def cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--outfile', '-o', default=sys.stdout, type=argparse.FileType('w'))
    parser.add_argument('infiles', nargs='+', type=Path)
    args = vars(parser.parse_args())

    main(**args)


if __name__ == '__main__':
    cli()
