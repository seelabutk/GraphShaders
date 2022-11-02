import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 1,
    editorid: 'editor',
    options: {
        debugTileBoundaries: 0,
        debugPartitionBoundaries: 0,
        doScissorTest: 1,
        dataset: 'NBER-Patents-eattr',
        pDepth: 0,
        node: `\
void node(in unit X, in unit Y, in float AppYear, in float GotYear, in int Cat1, in int Cat0, out flat float ay, out flat float gy, out flat int c1, out flat int c0) {
    fg_NodePosition = vec2(X, Y);
    fg_NodeDepth = fg_min(-X);
    ay = AppYear;
    gy = GotYear;
    c1 = Cat1 - 1;
    c0 = Cat0 - 1;
}`,
        edge: `\
#define I 1
#define J 1
void edge(in flat float ay, in flat float gy, in flat int c1, in flat int c0, in int CitingCat0, in int CitedCat0) {
    if (CitingCat0 != I || CitedCat0 != J) discard;
    //if ((fg_EdgeID + 0) % 1000 >= 100) discard;
    fg_EdgeColor = vec4(1./16.);
    fg_EdgeColor.r = float(gy - ay > 1.);
}`,
    },
});
