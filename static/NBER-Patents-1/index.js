import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 0,
    editorid: 'editor',
    options: {
        debugTileBoundaries: 0,
        debugPartitionBoundaries: 0,
        doScissorTest: 1,
        dataset: 'NBER-Patents-eattr',
        pDepth: 0,
        node: `\
void node(in unit x, in unit y, in unit appdate, in unit gotdate, in int nclass, in int cclass, out flat int c) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(x);
    c = cclass - 1;
}`,
        edge: `\
const vec3 colors[6] = vec3[6](
    vec3(0.86, 0.3712, 0.33999),
    vec3(0.82879, 0.86, 0.33999),
    vec3(0.33999, 0.86, 0.3712),
    vec3(0.33999, 0.82879, 0.86),
    vec3(0.3712, 0.33999, 0.86),
    vec3(0.86, 0.33999, 0.82879));
void edge(in flat int c, in int sc, in int tc) {
    fg_FragColor = vec4(colors[c], 0.1);
}`,
    },
});
