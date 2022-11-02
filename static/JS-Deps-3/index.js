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
void node(in unit X, in unit Y, in unit ReleaseDate, in int DevCount, in int Vulnerable, out flat float rd, out flat int dc, out flat int v) {
    fg_NodePosition = vec2(X, Y);
    fg_NodeDepth = fg_min(-ReleaseDate);
    rd = ReleaseDate;
    dc = DevCount;
    v = Vulnerable;
}`,
        edge: `\
#define LO 0.25
#define HI ((LO)+0.25)
void edge(in flat float rd, in flat int dc, in flat int v) {
    //if (!(gl_PrimitiveID % 1000 <= 500)) discard;
    if (!(LO <= rd && rd <= HI)) discard;
    fg_EdgeColor = vec4(1./16.);
    fg_EdgeColor.r = float(dc > 3);
    fg_EdgeColor.g = float(v > 0);
}`,
    },
});
