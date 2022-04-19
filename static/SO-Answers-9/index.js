import { Application } from './fg.js';

const node = `\
#define PI 3.1415926538
void node(in float x, in float y, in float id, out flat float i) {
    x = (1. + cos(2*PI*id)) / 2.;
    y = (1. + sin(2*PI*id)) / 2.;
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-id);
    i = id;
}`;

const mkedge = (lo, hi) => `\
void edge(in flat float i, in float date) {
    fg_Disregard = float(!(${lo.toFixed(2)} <= date && date < ${hi.toFixed(2)}));
    fg_FragColor = vec4(0.1);
    fg_FragColor.r = date - i > 0. ? date - i : 0.;
    fg_FragColor.b = i - date > 0. ? i - date : 0.;
}`;

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 1,
    editorid: 'editor',
    keyframes: [
        { node, edge: mkedge(0.10, 0.20), delay: 5 * 1000 },
        { node, edge: mkedge(0.20, 0.30), delay: 5 * 1000 },
        { node, edge: mkedge(0.30, 0.40), delay: 5 * 1000 },
        { node, edge: mkedge(0.40, 0.50), delay: 5 * 1000 },
        { node, edge: mkedge(0.50, 0.60), delay: 5 * 1000 },
        { node, edge: mkedge(0.60, 0.70), delay: 5 * 1000 },
        { node, edge: mkedge(0.70, 0.80), delay: 5 * 1000 },
        { node, edge: mkedge(0.80, 0.90), delay: 5 * 1000 },
        { node, edge: mkedge(0.90, 1.00), delay: 5 * 1000 },
    ],
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'so-answers',
        pDepth: 0,
        node,
        edge: mkedge(0.00, 0.10),
    },
});
