import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 6,
    editorid: 'editor',
    options: {
        debugTileBoundaries: 1,
        debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'knit-graph',
        pDepth: 10,
        node: `\
void node(in float x, in float y, in float date, in float _numMaint, in float cve, out float vulnerable) {
    vulnerable = cve;
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_min(-cve);
}`,
        edge: `\
void edge(in float vulnerable) {
    vec4 red = vec4(0.8, 0.1, 0.1, 0.1);
    vec4 green = vec4(0.0, 1.0, 0.0, 0.1);
    vec4 color = mix(red, green, vulnerable);
    fg_FragColor = color;
}`,
    },
});
