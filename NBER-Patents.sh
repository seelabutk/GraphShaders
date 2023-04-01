#!/usr/bin/env bash
exec \
    "$@" \
    -f X graph/NBER-Patents/NBER-Patents.node.x.f32.bin \
    -f Y graph/NBER-Patents/NBER-Patents.node.y.f32.bin \
    -f GotYear graph/NBER-Patents/NBER-Patents.node.gotyear.u32.bin \
    -f AppYear graph/NBER-Patents/NBER-Patents.node.appyear.u32.bin \
    -f Cat1 graph/NBER-Patents/NBER-Patents.node.cat1.u32.bin \
    -f Cat2 graph/NBER-Patents/NBER-Patents.node.cat2.u32.bin \
    -f element graph/NBER-Patents/NBER-Patents.edge.elements.2u32.bin \
    ##
