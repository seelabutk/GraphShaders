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
void node(in float x, in float y, in float id) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
}`,
        edge: `\
void edge(in float date) {
    fg_FragColor = vec4(0.1);
}`,
    },
});
