import { CodeJar } from 'https://unpkg.com/codejar@2.0.0/codejar.js';
import fg from './fg.js';

// Thanks https://gist.github.com/malthe/02350255c759d5478e89
function dedent(text) {
    // text = text.replace(new RegExp('^[ \t]+\n', ''), '');
    // text = text.replace(new RegExp('\n[ \t]+$', ''), '');

    var re_whitespace = /^([ \t]*)(.*)\n/gm;
    var l, m, i;

    while ((m = re_whitespace.exec(text)) !== null) {
        if (!m[2]) continue;

        if (l = m[1].length) {
            const iold = i;
            i = (i !== undefined) ? Math.min(i, l) : l;
            console.log({ iold, i, l });
        } else break;
    }

    if (i)
        text = text.replace(new RegExp('^[ \t]{' + i + '}(.*\n)', 'gm'), '$1');

    return text;
}

function indent(text, prefixOrCount, maybePrefix) {
    let count = 1;
    let prefix = '    ';

    if (prefixOrCount !== undefined && Number.isInteger(prefixOrCount)) {
        count = prefixOrCount;
        if (maybePrefix !== undefined) {
            prefix = maybePrefix;
        }
    } else if (prefixOrCount !== undefined) {
        prefix = prefixOrCount;
    }

    console.log({ text, prefix, count });

    prefix = prefix.repeat(count);

    return text.split('\n').map((d) => prefix + d).join('\n');
}

const makeFGCode = (options) => `\
fg({
    node: \`
${indent(dedent(options.node), 2)}
    \`,
    edge: \`
${indent(dedent(options.edge), 2)}
    \`,
})
`;

export default class Application {
    #options;
    #map;
    #jar;
    #oldTileLayer;
    #curTileLayer;
    #currentlyWaitingForTiles;
    #shouldRequestNewTilesWhenTilesAreLoaded;

    constructor({ mapid, center, zoom, editorid, options }) {
        // bind class methods to this
        this.update = this.update.bind(this);
        this.handleCodeChange = this.handleCodeChange.bind(this);
        this.handleTilesLoaded = this.handleTilesLoaded.bind(this);

        // create objects
        const map = L.map(mapid, {
            crs: L.CRS.Simple,
            tms: true,
            fadeAnimation: false,
            center,
            zoom,
        });

        const jar = new CodeJar(document.getElementById(editorid), Prism.highlightElement);

        // update member variables
        this.#options = options;
        this.#map = map;
        this.#jar = jar;
        this.#oldTileLayer = null;
        this.#curTileLayer = null;
        this.#currentlyWaitingForTiles = false;
        this.#shouldRequestNewTilesWhenTilesAreLoaded = false;

        // setup event listeners
        this.#jar.updateCode(makeFGCode(options));
        this.#jar.onUpdate(this.handleCodeChange);

        // kick everything off
        this.update({});
    }

    handleCodeChange() {
        // This is super confusing. The order of operations here is that we are
        // provided some code:
        //   const code = `fg({ node: 'TheNodeShader', edge: 'TheEdgeShader' })`;
        //
        // We want to execute this code and get access to node and edge shader
        // (i.e. the "options") and pass these options to the Application.update
        // function.
        //
        // The MDN documentation [1] suggests using an odd workaround to avoid
        // using eval. Ultimately, the code that gets executed will look like
        // this:
        //
        //   function foo() {
        //       "use strict";
        //       return function bar(fg) {
        //           fg({ node: 'TheNodeShader', edge: 'TheEdgeShader' });
        //       }
        //   }
        //   const bar = foo();
        //   bar(fg);
        //
        // The fg function passed to "bar" is responsible for calling
        // Application.update with its parameter.
        //
        // The last remaining complication is actualling following [1]'s advice
        // with creating a Function object instead of just calling eval.
        //
        // [1]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/eval#never_use_eval!

        // if we're already waiting, don't kick off another request
        if (this.#currentlyWaitingForTiles) {
            this.#shouldRequestNewTilesWhenTilesAreLoaded = true;
            return;
        }

        const code = this.#jar.toString();
        const foo = Function('"use strict";return function(fg) { return ' + code + ' };');
        const bar = foo();
        bar(this.update);
    }

    handleTilesLoaded() {
        if (this.#shouldRequestNewTilesWhenTilesAreLoaded) {
            setTimeout(this.handleCodeChange, 0);
        }

        this.#curTileLayer.off('load', this.handleTilesLoaded);
        if (this.#oldTileLayer !== null) {
            this.#map.removeLayer(this.#oldTileLayer);
            this.#oldTileLayer._map = {};
            this.#oldTileLayer = null;
        }

        this.#shouldRequestNewTilesWhenTilesAreLoaded = false;
        this.#currentlyWaitingForTiles = false;
    }

    update(options) {
        options = {
            ...this.#options,
            ...options,
        };

        const url = fg(options);
        const curTileLayer = new L.TileLayer(url, {
            tileSize: 256,
        });
        curTileLayer.addTo(this.#map);
        curTileLayer.bringToFront();

        this.#options = options;
        if (this.#curTileLayer !== null) {
            this.#oldTileLayer = this.#curTileLayer;
            this.#oldTileLayer.setUrl(L.Util.emptyImageUrl, true);
        }
        this.#curTileLayer = curTileLayer;
    }
};

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
