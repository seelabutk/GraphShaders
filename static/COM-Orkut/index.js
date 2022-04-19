import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 6,
    editorid: 'editor',
    options: {
        debugTileBoundaries: 1,
        debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'orkut',
        pDepth: 10,
        node: `\
void node(in float x, in float y, out float notUsed) {
    notUsed = 1;
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-x);
}`,
        edge: `\
void edge(in float notUsed) {
    fg_FragColor = vec4(0.8, 0.1, 0.1, 0.1);
}`,
    },
});
