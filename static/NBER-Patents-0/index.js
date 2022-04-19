import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 6,
    editorid: 'editor',
    options: {
        debugTileBoundaries: 0,
        debugPartitionBoundaries: 0,
        doScissorTest: 1,
        dataset: 'cit-Patents',
        pDepth: 10,
        node: `\
void node(in float x, in float y, in float appdate, in float gotdate, in float nclass, in float cclass) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
}`,
        edge: `\
void edge() {
    fg_FragColor = vec4(0.1);
}`,
    },
});
