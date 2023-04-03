#!/usr/bin/env bash

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)

exec GraphShaderTranspiler.py \
    -i "${root:?}/SO-Answers.gsp" \
    -f element "${root:?}/data/SO-Answers.edge.element.2u32.bin" \
    -f X       "${root:?}/data/SO-Answers.node.x.f32.bin"        \
    -f Y       "${root:?}/data/SO-Answers.node.y.f32.bin"        \
    -f When    "${root:?}/data/SO-Answers.edge.when.u32.bin"     \
    -e GS_OUTPUT "${root:?}/SO-Answers.jpg" \
    -e GS_TILE_WIDTH 2048 \
    -e GS_TILE_HEIGHT 2048 \
    -e GS_TILE_Z 1 \
    -e GS_TILE_X 0.5 \
    -e GS_TILE_Y 0.5 \
    ##
