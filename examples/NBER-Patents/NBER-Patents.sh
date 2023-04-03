#!/usr/bin/env bash

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)

exec GraphShaderTranspiler.py \
    -i "${root:?}/NBER-Patents.gsp" \
    -f element "${root:?}/data/NBER-Patents.edge.element.2u32.bin" \
    -f X       "${root:?}/data/NBER-Patents.node.x.f32.bin"        \
    -f Y       "${root:?}/data/NBER-Patents.node.y.f32.bin"        \
    -f GotYear "${root:?}/data/NBER-Patents.node.gotyear.u32.bin"  \
    -f AppYear "${root:?}/data/NBER-Patents.node.appyear.u32.bin"  \
    -f Cat1    "${root:?}/data/NBER-Patents.node.cat1.u32.bin"     \
    -f Cat2    "${root:?}/data/NBER-Patents.node.cat2.u32.bin"     \
    -e GS_OUTPUT "${root:?}/NBER-Patents.jpg" \
    -e GS_TILE_WIDTH 2048 \
    -e GS_TILE_HEIGHT 2048 \
    -e GS_TILE_Z 1 \
    -e GS_TILE_X 0.25 \
    -e GS_TILE_Y 0.5 \
    ##
