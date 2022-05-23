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
void node(in unit x, in unit y, in unit appdate, in unit gotdate, in int nclass, in int cclass, out float c, out flat float d) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-gotdate);
    c = float(cclass);
    d = gotdate * (gotdateMax - gotdateMin) + gotdateMin;
}`,
        edge: `\
const vec3 colors[6] = vec3[6](
    vec3(0., 0., 1.),
    vec3(0., 1., 0.),
    vec3(0., 1., 1.),
    vec3(1., 0., 0.),
    vec3(1., 0., 1.),
    vec3(1., 1., 0.));
vec3 interpolate(in float x, in float y, in float a) {
    const float minimum = min(x, y);
    const float maximum = max(x, y);
    const float ratio = (a - minimum) / (maximum - minimum);
    const vec3 xColor = colors[int(minimum)-1];
    const vec3 yColor = colors[int(maximum)-1];
    return mix(xColor, yColor, ratio);
}
#define I 0
const int SC[4] = { 2, 2, 5, 5 };
const int TC[4] = { 2, 5, 2, 5 };
#define SC SC[I]
#define TC TC[I]
#define J 0
#define DLO (1976.+J.)
#define DHI (1977.+J.)
void edge(in float c, in flat float d, in int sc, in int tc) {
    fg_Disregard = float(!(SC == sc && tc == TC));
    fg_Disregard += float(!(DLO <= d && d < DHI));
    fg_FragColor = vec4(0.1);
    //fg_FragColor.rgb = interpolate(float(sc), float(tc), c);
}`,
    },
});
