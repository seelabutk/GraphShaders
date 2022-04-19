import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 6,
    editorid: 'editor',
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'knit-graph',
        pDepth: 10,
        node: `\
void node(in float x, in float y, in float date, in float maintainers, in float cve) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
}`,
        edge: `\
void edge() {
    fg_FragColor = vec4(0.1);
}`,
    },
});
