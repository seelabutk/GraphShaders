import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 6,
    editorid: 'editor',
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'so-answers',
        pDepth: 10,
        node: `\
void node(in float x, in float y, in float id, in float self, out float accountCreated) {
    fg_NodePosition = vec2(x, y);
    fg_NodeDepth = fg_max(x);
    accountCreated = id;
    fg_NodeDepth = 1.0 - accountCreated;
}`,
        edge: `\
void edge(in float accountCreated) {
    fg_Disregard = 0.0;

    float account_max = 0.01;
    float account_min = 0.00;
    if (accountCreated > account_max || accountCreated < account_min) {
        fg_Disregard = 1.0;
    }

    vec3 cool = vec3(0.6, 0.749, 0.596);
    vec3 warm = vec3(0.878, 0.38, 0.282);
    vec3 color = mix(cool, warm, smoothstep(account_min, account_max, accountCreated));

    float seen = fg_EdgeData.x * 5.0;
    float seen_max = 0.10;
    float seen_min = 0.00;
    if (seen > seen_max || seen < seen_min) {
        fg_Disregard = 1.0;
    }

    fg_EdgeMutate = (1.0 - seen);
    fg_FragColor = vec4(color * clamp(smoothstep(seen_min, seen_max, seen), 0.1, 1.0), 1.0);
}`,
    },
});
