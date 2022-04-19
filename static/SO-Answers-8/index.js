import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 1,
    editorid: 'editor',
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'so-answers',
        pDepth: 0,
        node: `\
#define PI 3.1415926538
void node(in float x, in float y, in float id, out flat float i) {
    x = (1. + cos(2*PI*id)) / 2.;
    y = (1. + sin(2*PI*id)) / 2.;
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-id);
    i = id;
}`,
        edge: `\
#define LO 0.33
#define HI 0.34
void edge(in flat float i, in float date) {
    fg_Disregard = float(!(LO <= i && i < HI));
    fg_FragColor = vec4(0.1);
    fg_FragColor.r = date > i ? date - i : 0.;
    fg_FragColor.b = i > date ? i - date : 0.;
}`,
    },
});
