import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 2,
    editorid: 'editor',
    options: {
        debugTileBoundaries: 0,
        debugPartitionBoundaries: 0,
        doScissorTest: 1,
        dataset: 'NBER-Patents-eattr',
        pDepth: 0,
        node: `\
void node(in unit x, in unit y, in unit appdate, in unit gotdate, in int nclass, in int cclass) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
}`,
        edge: `\
void edge(in int sc, in int tc) {
    fg_FragColor = vec4(0.1);
}`,
    },
});
