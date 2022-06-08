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
    float len = sqrt(x * x + y * y);
    x = x / len;
    y = y / len;
    x *= gotdate;
    y *= gotdate;
    x = x / 2. + 0.5;
    y = y / 2. + 0.5;
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(gotdate);
    c = cclass - 1;
    d = gotdate;
}`,
        edge: `\
const vec3 Blues[5] = vec3[5](
    vec3(239., 243., 255.),
    vec3(189., 215., 231.),
    vec3(107., 174., 214.),
    vec3( 49., 130., 189.),
    vec3(  8.,  81., 156.));
const vec3 Greens[5] = vec3[5](
    vec3(237., 248., 233.),
    vec3(186., 228., 179.),
    vec3(116., 196., 118.),
    vec3( 49., 163.,  84.),
    vec3(  0., 109.,  44.));
const vec3 Greys[5] = vec3[5](
    vec3(247., 247., 247.),
    vec3(204., 204., 204.),
    vec3(150., 150., 150.),
    vec3( 99.,  99., 99.),
    vec3( 37.,  37., 37.));
const vec3 Oranges[5] = vec3[5](
    vec3(254., 237., 222.),
    vec3(253., 190., 133.),
    vec3(253., 141.,  60.),
    vec3(230.,  85.,  13.),
    vec3(166.,  54.,  3.));
const vec3 Purples[5] = vec3[5](
    vec3(242., 240., 247.),
    vec3(203., 201., 226.),
    vec3(158., 154., 200.),
    vec3(117., 107., 177.),
    vec3( 84.,  39., 143.));
const vec3 Reds[5] = vec3[5](
    vec3(254., 229., 217.),
    vec3(252., 174., 145.),
    vec3(251., 106.,  74.),
    vec3(222.,  45.,  38.),
    vec3(165.,  15.,  21.));
const vec3 colors[6][5] = vec3[6][5](
    Greys,
    Blues,
    Greens,
    Oranges,
    Purples,
    Reds);
#define SC 4
void edge(in float c, in flat float d, in int sc, in int tc) {
    //fg_Disregard = float(!(sc == SC && tc != SC));
    int i = tc == SC ? 0 : (tc < SC ? tc : tc - 1);
    float r = d;
    fg_FragColor = vec4(colors[i][int(r * 5.)] / 255., 0.1);
}`,
    },
});
