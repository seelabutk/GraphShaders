import { Application } from './fg.js';

new Application({
    mapid: 'map',
    center: [-128, 128],
    zoom: 1,
    editorid: 'editor',
    options: {
        // debugTileBoundaries: 1,
        // debugPartitionBoundaries: 1,
        doScissorTest: 1,
        dataset: 'SO-Answers-edgetime',
        pDepth: 0,
        node: `\
void node(in unit X, in unit Y, in unit FirstContribution, in unit LastContribution, in unit InDegree, in unit OutDegree, out flat float fc, out flat float lc, out flat float id, out flat float od) {
    fg_NodePosition = vec2(X, Y);
    fg_NodeDepth = fg_min(-FirstContribution);
    fc = FirstContribution;
    lc = LastContribution;
    id = InDegree;
    od = OutDegree;
}`,
        edge: `\

const vec3 Paired4Class[] = vec3[](
    vec3(166., 206., 227.) / 255.,
    vec3( 31., 120., 180.) / 255.,
    vec3(178., 223., 138.) / 255.,
    vec3( 51., 160.,  44.) / 255.);

void edge(in flat float fc, in flat float lc, in flat float id, in flat float od, in unit CurrentContribution, in unit SenderLongevity, in unit ReceiverLongevity) {
    if (!(0.25 <= fc && fc <= 0.50)) discard;
    fg_EdgeColor = vec4(1./16.);
    int index = 0;
    index += 2 * int(id > od); // blue (false) vs green (true)
    index += 1 * int(SenderLongevity >= ReceiverLongevity); // light (false) vs dark (true)
    fg_EdgeColor.rgb = Paired4Class[index];
}`,
    },
});
