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
void node(in float x, in float y, in float appdate, in float gotdate, in float nclass, in float cclass, out float c1, out float c2, out float c3) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(appdate);
    vec3 color = uCat6[int(5.0*cclass)];
    c1 = color.r;
    c2 = color.g;
    c3 = color.b;
}`,
        edge: `\
void edge(in float c1, in float c2, in float c3) {
    fg_FragColor = vec4(fg_FragDepth * vec3(c1, c2, c3), 1.0);
}`,
    },
});
