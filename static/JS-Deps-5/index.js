import { Application } from './fg.js';

const nodes = [
    `\
void node(in float x, in float y, in float date, in float maintainers, in float cve) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
}`,
    `\
void node(in float x, in float y, in float date, in int maintainers, in int vulnerable, out float v) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
    v = vulnerable;
}`,
    `\
void node(in float x, in float y, in float date, in int maintainers, in int vulnerable, out float v) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-vulnerable);
    v = vulnerable;
}`,
    `\
void node(in float x, in float y, in float date, in int maintainers, in int vulnerable, out float v, out float m) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-vulnerable);
    v = vulnerable;
    m = maintainers;
}`,
    `\
void node(in float x, in float y, in float date, in int maintainers, in int vulnerable, out float v, out float m) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-vulnerable);
    v = vulnerable;
    m = maintainers;
}`,
];

const edges = [
    `\
void edge() {
    fg_FragColor = vec4(0.1);
}`,
    `\
void edge(in float v) {
    fg_FragColor = vec4(0.1);
    fg_FragColor.r = float(v > 0.0);
}`,
    `\
void edge(in float v) {
    fg_FragColor = vec4(0.1);
    fg_FragColor.r = float(v > 0.0);
}`,
    `\
void edge(in float v, in float m) {
    fg_FragColor = vec4(0.1);
    fg_FragColor.g = float(m > 4.0);
}`,
    `\
void edge(in float v, in float m) {
    fg_FragColor = vec4(0.1);
    fg_FragColor.r = float(v > 0.0);
    fg_FragColor.g = float(m > 4.0);
}`,
];

clearAllMeasurements();

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 6,
    editorid: 'editor',
    keyframes: [
        /*  5 */ { offset: [+0, +0, 0], node: nodes[1], edge: edges[1] },
        /* 10 */ { offset: [+0, +0, 0], node: nodes[2], edge: edges[2] },
        /* 15 */ { offset: [+0, +0, 0], node: nodes[3], edge: edges[3] },
        /* 20 */ { offset: [+0, +0, 0], node: nodes[4], edge: edges[4] },

        /* 25 */ { offset: [+0, +0, 1] },
        /* 30 */ { offset: [+0, +0, 2] },
        /* 35 */ { offset: [+0, +0, 3] },

        /* 40 */ { offset: [+1, +0, 3] },
        /* 45 */ { offset: [+0, +1, 3] },
        /* 50 */ { offset: [-1, +0, 3] },
        /* 55 */ { offset: [+0, -1, 3] },

        /* 60 */ { offset: [+0, -1, 2] },

        /* 65 */ { offset: [-1, +0, 2] },
        /* 70 */ { offset: [+0, +1, 2] },
        /* 75 */ { offset: [+1, +0, 2] },
        /* 80 */ { offset: [+0, +0, 2] },

        /* 85 */ { offset: [+0, +0, 1] },
        /* 90 */ { offset: [+0, +0, 0] },
        /* 95 */ { callback: () => { saveMeasurements(); } },
    ],
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doOcclusionCulling: 0,
        doScissorTest: 1,
        dataset: 'knit-graph',
        pDepth: 10,
        node: nodes[0],
        edge: edges[0],
    },
});
