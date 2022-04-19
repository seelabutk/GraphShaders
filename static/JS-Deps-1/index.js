import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 6,
    editorid: 'editor',
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doOcclusionCulling: 0,
        doScissorTest: 1,
        dataset: 'knit-graph',
        pDepth: 10,
        node: `\
void node(in float x, in float y, in float date, in int maintainers, in int vulnerable, out float v) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
    v = vulnerable;
}`,
        edge: `\
void edge(in float v) {
    fg_FragColor = vec4(0.1);
    fg_FragColor.r = float(v > 0.0);
}`,
    },
});
