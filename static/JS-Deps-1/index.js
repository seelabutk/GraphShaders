import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 2,
    editorid: 'editor',
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'JS-Deps-filtered',
        pDepth: 0,
        node: `\
void node(in unit X, in unit Y, in unit ReleaseDate, in unit DevCount, in int Vulnerable, out flat float rd, out flat float dc, out flat int v) {
    fg_NodePosition = vec2(X, Y);
    fg_NodeDepth = fg_min(-ReleaseDate);
    rd = ReleaseDate;
    dc = DevCount;
    v = Vulnerable;
}`,
        edge: `\
void edge(in flat float rd, in flat float dc, in flat int v) {
    if (!(gl_PrimitiveID % 1000 <= 50)) discard;
    fg_EdgeColor = vec4(0.1);
}`,
    },
});
