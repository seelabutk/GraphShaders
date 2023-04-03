
#!/usr/bin/env bash

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)

exec GraphShaderTranspiler.py \
    -i "${root:?}/JS-Deps.gsp" \
    -f element "${root:?}/data/JS-Deps.edge.elements.2u32.bin" \
    -f X "${root:?}/data/JS-Deps.node.x.f32.bin" \
    -f Y "${root:?}/data/JS-Deps.node.y.f32.bin" \
    -f Date "${root:?}/data/JS-Deps.node.date.u32.bin" \
    -f Devs "${root:?}/data/JS-Deps.node.nmaintainers.u32.bin" \
    -f Vuln "${root:?}/data/JS-Deps.node.cve.u32.bin" \
    -e GS_OUTPUT "${root:?}/JS-Deps.jpg" \
    -e GS_TILE_WIDTH 2048 \
    -e GS_TILE_HEIGHT 2048 \
    -e GS_TILE_Z 1 \
    -e GS_TILE_X 0.5 \
    -e GS_TILE_Y 0.5 \
    "$@" \
    ##
