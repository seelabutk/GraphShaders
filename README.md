# Graph Shaders for Visualizing Large Graphs

This repository contains the code to visualize large graphs using Graph Shader
Programs (`.GSP`).

The Graph Shaders (GS) system architecture is comprised of two main pieces:

- The GS Engine is a C++ program that loads graph data from disk and configures
  OpenGL to render specially designed shaders.

- The GS Transpiler is a Python program that parses `.GSP` programs and is
  configured using `#pragma` statements.

The remainder of this repository is a set of example scripts.

## Building

In the simplest case, the GS Engine can be compiled using CMake like any other
CMake program.

```console
$ cmake -Bbuild
$ cmake --build build
$ cmake --install build
```

We also provide a Dockerfile for making setup easier. It can be accessed
directly by calling `docker`, but we also have a helper script `go.sh` that
simplifies some of the repeated arguments.

```console
$ ./go.sh docker build
$ ./go.sh docker start
$
$ ./go.sh docker exec ./go.sh gs configure
$ ./go.sh docker exec ./go.sh gs build
$ ./go.sh docker exec ./go.sh gs install
```

## Examples

We provide several example GS Programs. The data can be downloaded from our
server.

```console
$ wget -O examples/JS-Deps/JS-Deps.data.tar.gz https://accona.eecs.utk.edu/JS-Deps.data.tar.gz
$ wget -O examples/SO-Answers/SO-Answers.data.tar.gz https://accona.eecs.utk.edu/SO-Answers.data.tar.gz
$ wget -O examples/NBER-Patents/NBER-Patents.data.tar.gz https://accona.eecs.utk.edu/NBER-Patents.data.tar.gz
$
$ (cd examples/JS-Deps && tar xvf JS-Deps.data.tar.gz)
$ (cd examples/SO-Answers && tar xvf SO-Answers.data.tar.gz)
$ (cd examples/NBER-Patents && tar xvf NBER-Patents.data.tar.gz)
```

If you are using our `go.sh` scripts, then you can execute our example scripts:

```console
$ ./go.sh docker exec ./go.sh gs exec ./examples/JS-Deps/JS-Deps.sh
$ ./go.sh docker exec ./go.sh gs exec ./examples/SO-Answers/SO-Answers.sh
$ ./go.sh docker exec ./go.sh gs exec ./examples/NBER-Patents/NBER-Patents.sh
```

Otherwise, you should make sure that GraphShaderEngine and
GraphShaderTranspiler.py are within your path.

### Example: JS-Deps

![](examples/JS-Deps/JS-Deps.example.jpg)

### Example: SO-Answers

![](examples/SO-Answers/SO-Answers.example.jpg)

### Example: NBER-Patents

![](examples/NBER-Patents/NBER-Patents.example.jpg)
