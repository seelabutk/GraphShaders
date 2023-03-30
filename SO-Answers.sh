#!/usr/bin/env bash
exec \
    "$@" \
    -f X graph/SO-Answers/SO-Answers.node.x.f32.bin \
    -f Y graph/SO-Answers/SO-Answers.node.y.f32.bin \
    -f element graph/SO-Answers/SO-Answers.edge.elements.2u32.bin \
    -f When graph/SO-Answers/SO-Answers.edge.when.u32.bin \
    ##
