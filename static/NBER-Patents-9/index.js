import { Application } from './fg.js';

const node = `\
void node(in unit x, in unit y, in unit appdate, in unit gotdate, in int nclass, in int cclass) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
}`;

const edge = (sc) => `\
const vec3 colors[6] = vec3[6](
    vec3(0.86, 0.3712, 0.33999),
    vec3(0.82879, 0.86, 0.33999),
    vec3(0.33999, 0.86, 0.3712),
    vec3(0.33999, 0.82879, 0.86),
    vec3(0.3712, 0.33999, 0.86),
    vec3(0.86, 0.33999, 0.82879));
void edge(in int sc, in int tc) {
    fg_Disregard = float(!(sc > ${sc}));
    fg_FragColor = vec4(0.1);
    fg_FragColor.rgb = colors[sc-1];
}`;

clearAllMeasurements();

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 2,
    editorid: 'editor',
    keyframes: [
        { node, edge: edge(4) },
        { node, edge: edge(3) },
        { node, edge: edge(2) },
        { node, edge: edge(1) },
        { node, edge: edge(0) },

        /*  0 */ //{ offset: [+0, +0, +0] },
        /*  5 */ { offset: [+0, +0, +1] },
        /* 10 */ { offset: [+0, +0, +2] },
        /* 15 */ { offset: [+0, +0, +3] },

        /* 20 */ { offset: [+1, +0, +3] },
        /* 25 */ { offset: [+0, +1, +3] },
        /* 30 */ { offset: [-1, +0, +3] },
        /* 35 */ { offset: [+0, -1, +3] },

        /* 40 */ { offset: [+0, -1, +2] },
        /* 45 */ { offset: [+1, +0, +2] },
        /* 50 */ { offset: [+0, +1, +2] },
        /* 55 */ { offset: [-1, +0, +2] },

        /* 60 */ { offset: [-1, +0, +1] },
        /* 65 */ { offset: [+0, -1, +1] },
        /* 70 */ { offset: [+1, +0, +1] },
        /* 75 */ { offset: [+0, +1, +1] },
        /* 80 */ { callback: () => { saveMeasurements(); } },
    ],
    options: {
        debugTileBoundaries: 0,
        debugPartitionBoundaries: 0,
        doScissorTest: 1,
        dataset: 'NBER-Patents-eattr',
        pDepth: 0,
        node,
        edge: edge(5),
    },
});
