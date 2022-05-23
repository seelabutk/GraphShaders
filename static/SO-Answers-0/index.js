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
        dataset: 'SO-Answers-nodetime',
        pDepth: 0,
        node: `\
void node(in unit x, in unit y, in unit wmin, in unit wmax) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
}`,
        edge: `\
void edge(in unit when) {
    fg_FragColor = vec4(0.1);
}`,
    },
});
