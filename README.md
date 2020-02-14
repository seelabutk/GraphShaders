# Fast Graph

## To run it

```console
$ cmake -S. -Bbuild
$ make -C build
$ build/main
```

This will run the program which currently loads
a graph from the filesystem
in CSV format
and partitions it into several "RC"s.
These RCs are lists of edges,
in this case,
as edge IDs/numbers.

## To run the tests

```console
$ cmake -S. -Bbuild
$ make -C build
$ build/test_fg
```

## Todos

- MVP
  - [x] Ray trace to assign edges to tiles
  - [x] Partition with Z-Order Curve to map tiles to RCs
  - [ ] Render with OpenGL to draw RCs/tiles to an image
  - [ ] Wrap renderer with simple web server and interface with Leaflet
- Full E2E
  - [ ] Get Audris' data (R dependencies on WoC)
  - [ ] Benchmark renderer across different number of RCs
- To be decided
  - [ ] Determine how to factor in time/deltas
  - [ ] Look into user-defined "shaders" and "filters"
