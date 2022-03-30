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
void node(in float x, in float y, in float appdate, in float gotdate, in float nclass, in float cclass, out vec3 color) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(appdate);
    color = uCat6[int(5.0*cclass)];
}`,
        edge: `\
void edge(in vec3 color) {
    fg_FragColor = vec4(fg_FragDepth * color, 1.0);
}`,
    },
});
