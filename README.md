# Fast Graph

Fast tiled graph rendering as a web service.


## Development

Fast Graph (FG) is compiled and run with Docker using a helper script called
`go.sh`. The script is intended to be used as a shorthand to type smaller
commands, and can be modified as needed for different tasks. `go.sh` has many
variables defined at the top that influence the rest of the execution. These
variables can either be defined within the `go.sh` file, or a new file called
`env.sh` can be created and its variables will override those in `go.sh`.

FG's code is split into 2 parts: the backend web server in C and the frontend
interface in HTML/JavaScript. The server receives requests and parses the
requested URL to extract different rendering parameters such as the dataset to
render or the shader code to run. The frontend takes user interactions and uses
them to create new URLs to request from the backend, and then uses the
responses to create the overall visualization.

To fascillitate re-interpretable logging (capable of deciphering what actions
led to other actions), FG uses a custom library called "MAB/log". This library
has a few concepts: "actions" log the start & end times and a message for each;
"messages" log their time, a variable name, and a message; the current logging
state is an "info" that can be forwarded and continued in a different process.


### Building

The files that affect the build process are: `go.sh` & `env.sh` (variables:
`$target` and `$tag`), `Dockerfile`. The code files that get compiled are:
`src/server.c`, `src/server.h`, `src/base64.h`, `src/MAB/log.h`,
`src/stb_image_write.h`, and `src/glad/glad.h`.

```console
$ ./go.sh build  # => create the Docker container image
```


### Running

The files that affect the "run" process are: `go.sh` & `env.sh` (main
variables: `$port`, `$cwd`, `$data`, `$name`, `$constraints`, and `$replicas`).

```
$ ./go.sh fg     # => run one FG instance using the Docker container image
$ ./go.sh create-fg  # => run $replicas number of FG instances
$ ./go.sh destroy    # => stop the instances created with create-fg
```


### Backend Overview

The backend is driven by a library called MicroHTTPD. This is an event-driven
library that calls a single handler function which is called "answer" in
`src/server.c`. This "answer" function does simplistic routing to call other
functions. The most important of these are: `Index`, `Static`, and `Tile`.

The Index and Static functions are written very similarly. They read a file
from disk, convert it into a MicroHTTPD data structure, and then send it back
to the client. Because of how they are written, changes to files after the
server has started *will* be reflected in the HTTP responses.

The Tile function is where the main URL parsing code is. It expects a URL of
the form `/tile/:dataset/:z/:x/:y/:options`. Where `:dataset` is a string such
that `$PWD/data/:dataset/nodes.dat` and `$PWD/data/:dataset/edges.dat` exist.
`:z`, `:x`, and `:y` are floats that refer to the zoom, x translation, and y
translation, respectively.

`:options` is a key-value dictionary where every is separated by commas (e.g.
`key1,value1,key2,value2`). The values can be base64-encoded if they are
prefixed with `base64:` (e.g. `key,base64:abcedefa`). Due to a bug, there has
to be a leading comma in options (e.g. `,key1,value1,key2,value2`). The options
are where most rendering parameters are encoded. These include: `vert`, `frag`,
`dcIdent`, `dcIndex`, `dcMult`, `dcOffset`, `dcMinMult`, `dcMaxMult`. These
variables get parsed and saved to global variables of the same name, prefixed
with an underscore, and processed by the "render" function.

The "render" function is where all the real work happens. It initializes
OpenGL, loads the dataset, creates OpenGL data structures, handles the "Data
Combinator" ("DC"), and renders to a flat buffer. This happens in a loop and is
signaled by the "Tile" function, such that it only processes one request at a
time. There are some simplistic measures to reduce the amount of extra work
that needs to happen (e.g. don't reload the data if the "dataset" parameter
hasn't changed). As such, the code is structured in a way where if an earlier
part changes, then later parts will also re-run (e.g. if the dataset changes,
the OpenGL buffers also need to update).

The "Data Combinator" functionality is intended to control the ordering of the
rendering. In an ideal world, this would be a simple function that gets
evaluated for every edge, the resulting evaluation will be used to sort every
edge, and then the sorted edges are rendered. In practice, it is hard to run
user code efficiently without enforcing some constraints, and the constraint
enforced here is to only allow linear combinations of edge/node attributes. For
instance, "1.0 * Date + 0" is allowed, but "Date * HasVulnerability" is not
(instead, apply a priority to one or the other, such as "1.0 * Date + 1000.0 *
HasVulnerability"). Each "dc" option is actually an array of values, such that
an edge's "depth" is defined as the following pseudocode:

```
depth = 0
for (int i=0; i<MAX; ++i) {
    sourceDepth = dcMult[i] * SOURCE NODE ATTRIBUTE + dcOffset[i];
    targetDepth = dcMult[i] * TARGET NODE ATTRIBUTE + dcOffset[i];
    depth += dcMinMult * MINIMUM(sourceDepth, targetDepth);
    depth += dcMaxMult * MAXIMUM(sourceDepth, targetDepth);
}
```

The goal of the dcMinMult and dcMaxMult is to enable 3 types of operations:
keep only the minimum calculated depth, keep only the maximum calculated depth,
keep the average of both depths. This is necessary because an edge actually has
two associated "depths": the depth of the first node and the depth of the
second node. And hence, some decision must be made as to which depth. This is
all controlled by the frontend JavaScript code which can smartly determine the
values of each parameter from an equation.

The "frag" and "vert" options are simple OpenGL shaders. The vertex shader has
access to attributes named `aNode1`, `aNode2`, etc, and uniforms named
`uTranslateX`, `uTranslateY`, `uScale`, and `uNodeMin1`, `uNodeMax1`, etc.


### Frontend Overview

The core of the frontend is the Leaflet.js map that is used to make requests to
the backend server. As different parts of the code panel are changed, the URLs
that Leaflet requests also change. The other parts of the frontend include the
"makeDC" function to convert a mathematical expression into a format the
backend server can use, and the "makeUrlTemplate" function to turn all the
frontend state into a URL that the backend can parse.

There is an equivalent JavaScript version of the backend C logging library.
This JavaScript version is also capable of starting actions and having them
continued in the backend server. This forwarding is done by the "LoggingTileLayer".

The last important function is the "runCode" one that takes the user-written
source code and calls eval on it. This kicks off the rest of the update
process and updates the Leaflet tile layer.


## Current Problems and Future Todos

Currently, every FG instance loads the entirety of the dataset and that
entirety is used for every render, even if the current requests are all zoomed
in and only actually use a smaller portion of the dataset. Every time the zoom
level is changed, the dataset should be partitioned so that there is a lookup
from (zoom, x, y) to a set of edges. In terms of the backend code, this should
happen in the `INIT_PARTITION` part of the code which happens just after the
node and edge data is loaded and just before the Data Combinator is processed.

Currently, every FG application/dataset has their source code saved in one
`index.html` file that is symlinked manually to switch between applications.
Instead, the frontend code should be split into three files: first, a general `fg.js`
library that has a function to initialize FG with a default vertex and fragment
shader, a function to update the vertex and fragment shader source code, and
then a function to generate a URL for use with leaflet; second, a
per-application/per-dataset `app.js` that manages Leaflet and the code panel,
and calls into `fg.js` as needed; third, one or more `index.html` files that
load in the appropriate `fg.js` and `app.js` libraries. This is only a
suggestion, feel free to do whatever is needed.

Currently, there isn't any effective log parsing and it's all just kind of
manually defined. Instead, we should have a script in the repository that
processes the logs and generates statistics that we find interesting. This can
be done once the other features are in and we are ready to do performance
testing.
