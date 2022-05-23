import { CodeJar } from 'https://unpkg.com/codejar@2.0.0/codejar.js';

/** API Utilities */

function base64encode(s) {
    if (s instanceof Float32Array || s instanceof Uint32Array) {
        return base64encode(String.fromCharCode.apply(null, new Uint8Array(s.buffer)));
    }

    return `base64:${btoa(s)}`;
}

function makeVertexVariables(vert) {
    const match = vert.match(/void node\(([^)]+)\) {/);
    if (!match) {
        return {};
    }

    const variables = [];

    for (const parameter of match[1].split(',')) {
        const match = parameter.match(/\s*in (?:unit|float|int) ([a-zA-Z_]+)\s*/);
        if (!match) continue;

        variables.push(match[1]);
    }

    return {
        variables,
    };
}

let _makeDC_previousDepthExpression = null;
let _makeDC_previousDCIdent = 1337;
function makeDC(vert, variables) {
	const dc = {
        ident: _makeDC_previousDCIdent,
		index: [],
		mult: [],
		offset: [],
		combine: 0.5,
	};

    const match = vert.match(/fg_NodeDepth = (.*);/);
    console.log({ match });
    const thedepth = match !== null ? match[1] : 'fg_min(x)';
    console.log({ thedepth });

    if (_makeDC_previousDepthExpression !== thedepth) {
        dc.ident = ++_makeDC_previousDCIdent;
    }
    _makeDC_previousDepthExpression = thedepth;

	const code = math.compile(thedepth);

	const scope = {
		fg_min: (v) => v,
		fg_max: (v) => v,
		fg_mean: (v) => v,
	};
	variables.forEach((v) => { scope[v] = 0.0; });

	try {
		const combine = code.evaluate({
            ...scope,
			fg_min: () => 'min',
			fg_max: () => 'max',
			fg_mean: () => 'mean',
		});
		if (combine === 'min') dc.combine = 0.0;
		else if (combine === 'max') dc.combine = 1.0;
		else if (combine === 'mean') dc.combine = 0.5;
		else {
			console.error('depth expression needs min, max, or mean');
			return {};
		}
	} catch (e) {
		console.error('syntax error in depth expression', e);
		return {};
	}

	variables.forEach((variable, i) => {
		try {
			const at0 = code.evaluate({ ...scope, [variable]: 0.0 });
			const at1 = code.evaluate({ ...scope, [variable]: 1.0 });
			const at2 = code.evaluate({ ...scope, [variable]: 2.0 });
			const mult01 = at1 - at0;
			const mult02 = (at2 - at0) / 2.0;
			console.log({ variable, at0, at1, at2, mult01, mult02 });
			if (Math.abs(at0 - at1) < 0.01) {
				console.log('variable not used' + variable);
				return {};
			}
			if (Math.abs(mult01 - mult02) > 0.01) {
				console.error('nonlinear function involving ' + variable);
				return {};
			}

			const offset = dc.offset.length === 0 ? at0 : 0.0;
			const mult = mult01;

			dc.index.push(i + 1);
			dc.mult.push(mult);
			dc.offset.push(offset);
		} catch (e) {
			//console.warn({ variable, e });
		}
	});

	dc.index.push(0);

	return {
        dcIdent: dc.ident,
        dcIndex: new Uint32Array(dc.index),
        dcMult: new Float32Array(dc.mult),
        dcOffset: new Float32Array(dc.offset),
        dcMinMult: 1.0 - dc.combine,
        dcMaxMult: dc.combine,
    };
}


/** API */

export default function fg(options) {
    let url = '../tile';

    // defaults
    options = {
        z: '{z}',
        x: '{x}',
        y: '{y}',
        ...options,
    };

    if ('dataset' in options) {
        url += `/${options.dataset}`;
    } else throw 'dataset is required';

    if ('z' in options) {
        url += `/${options.z}`;
    } else throw 'z is required';

    if ('x' in options) {
        url += `/${options.x}`;
    } else throw 'x is required';

    if ('y' in options) {
        url += `/${options.y}`;
    } else throw 'y is required';

    if ('node' in options) {
        url += `/,vert,${base64encode(options.node)}`

        options = {
            ...makeVertexVariables(options.node),
            ...options,
        };
    } else throw 'node is required';

    if ('node' in options && 'variables' in options) {
        options = {
            ...makeDC(options.node, options.variables),
            ...options,
        };
    } else throw 'node and variables required';

    if ('edge' in options) {
        url += `,frag,${base64encode(options.edge)}`;
    } else throw 'edge is required';

    if ('pDepth' in options) {
        url += `,pDepth,${options.pDepth}`;
    } else throw 'pDepth is required';

    if ('dcIdent' in options) {
        url += `,dcIdent,${options.dcIdent}`;
    } else throw 'dcIdent is required';

    if ('dcIndex' in options) {
        url += `,dcIndex,${base64encode(options.dcIndex)}`;
    } else throw 'dcIndex is required';

    if ('dcMult' in options) {
        url += `,dcMult,${base64encode(options.dcMult)}`;
    } else throw 'dcMult is required';

    if ('dcOffset' in options) {
        url += `,dcOffset,${base64encode(options.dcOffset)}`;
    } else throw 'dcOffset is required';

    if ('dcMinMult' in options) {
        url += `,dcMinMult,${base64encode(options.dcMinMult)}`;
    } else throw 'dcMinMult is required';

    if ('dcMaxMult' in options) {
        url += `,dcMaxMult,${base64encode(options.dcMaxMult)}`;
    } else throw 'dcMaxMult is required';

    if ('logGLCalls' in options) {
        url += `,logGLCalls,${options.logGLCalls}`;
    }

    if ('doOcclusionCulling' in options) {
        url += `,doOcclusionCulling,${options.doOcclusionCulling}`;
    }

    if ('doScissorTest' in options) {
        url += `,doScissorTest,${options.doScissorTest}`;
    }

    if ('debugTileBoundaries' in options) {
        url += `,debugTileBoundaries,${options.debugTileBoundaries}`;
    }

    if ('debugPartitionBoundaries' in options) {
        url += `,debugPartitionBoundaries,${options.debugPartitionBoundaries}`;
    }

    return url;
}


/** Application Utilities */

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

/** Utils */

export const MeasuringTileLayer = L.TileLayer.extend({
    statics: {
        _measurements: [],

        _activeMeasurements: new Map(),

        _begin: null,

        clearMeasurements() {
            MeasuringTileLayer._measurements.length = 0;
        },

        getMeasurements() {
            const measurements = [...MeasuringTileLayer._measurements];
            // const end = performance.now();
            // for (const start of MeasuringTileLayer._activeMeasurements.values()) {
            //     measurements.push({ status: 'active', dt: end - start });
            // }
            return measurements;
        },

        _reportTile(tile, status) {
            if (tile.src.startsWith('data:')) return;
            if (!MeasuringTileLayer._activeMeasurements.has(tile)) {
                throw `tile ${tile.src} not recognized`;
            }
            const start = MeasuringTileLayer._activeMeasurements.get(tile);
            const end = performance.now();
            MeasuringTileLayer._measurements.push({ elapsed: start - MeasuringTileLayer._begin, status, dt: end - start });
            MeasuringTileLayer._activeMeasurements.delete(tile);
        },

        _reportTileCreate(tile) {
            if (tile.src.startsWith('data:')) return;
            if (MeasuringTileLayer._activeMeasurements.has(tile)) {
                throw `tile ${tile.src} already being tracked`;
            }
            const now = performance.now();
            if (MeasuringTileLayer._begin === null) {
                MeasuringTileLayer._begin = now;
            }
            MeasuringTileLayer._activeMeasurements.set(tile, now);
        },

        _reportTileLoad(tile) {
            MeasuringTileLayer._reportTile(tile, 'success');
        },

        _reportTileError(tile) {
            MeasuringTileLayer._reportTile(tile, 'error');
        },

        _reportTileAbort({ tile }) {
            MeasuringTileLayer._reportTile(tile, 'abort');
        },

    },

    initialize(url, options) {
        L.TileLayer.prototype.initialize.call(this, url, options);
        this.on('tileabort', MeasuringTileLayer._reportTileAbort);
    },

    createTile: function (coords, done) {
        const tile = L.TileLayer.prototype.createTile.call(this, coords, done);
        MeasuringTileLayer._reportTileCreate(tile);
        L.DomEvent.on(tile, 'load', L.Util.bind(MeasuringTileLayer._reportTileLoad, null, tile));
        L.DomEvent.on(tile, 'error', L.Util.bind(MeasuringTileLayer._reportTileError, null, tile));
        return tile;
    },
});
window.MeasuringTileLayer = MeasuringTileLayer;


L.Map.prototype.panToOffset = function (latlng, offset, options) {
    var x = this.latLngToContainerPoint(latlng).x - offset[0]
    var y = this.latLngToContainerPoint(latlng).y - offset[1]
    var point = this.containerPointToLatLng([x, y])
    return this.setView(point, this._zoom, { pan: options })
}


L.Map.prototype.setViewOffset = function (latlng, offset, targetZoom) {
    var targetPoint = this.project(latlng, targetZoom).subtract(offset),
    targetLatLng = this.unproject(targetPoint, targetZoom);
    return this.setView(targetLatLng, targetZoom);
}


/** Application */

export class Application {
    #options;
    #map;
    #jar;
    #oldTileLayer;
    #curTileLayer;
    #oldCode;
    #currentlyWaitingForTiles;
    #shouldRequestNewTilesWhenTilesAreLoaded;
    #doMeasurements;
    #center;
    #zoom;

    constructor({ mapid, center, zoom, editorid, keyframes, options }) {
        window.App = this;

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
        this.#oldCode = null;
        this.#doMeasurements = options.doMeasurements === undefined ? true : options.doMeasurements;
        this.#center = center;
        this.#zoom = zoom;

        // setup event listeners
        this.#jar.updateCode(makeFGCode(options));
        this.#jar.onUpdate(this.handleCodeChange);

        if (keyframes) {
            this.startAnimation(keyframes);
        }

        // kick everything off
        this.update({});
    }

    startAnimation(keyframes) {
        const w = +window.innerWidth/2;
        const h = +window.innerHeight/2;

        for (let i=0, totalDelay = 0; i<keyframes.length; ++i) {
            const keyframe = keyframes[i];
            setTimeout(() => {
                if (keyframe.offset !== undefined) {
                    this.#map.setViewOffset(
                        this.#center,
                        [w * keyframe.offset[0], h * keyframe.offset[1]],
                        this.#zoom + keyframe.offset[2],
                    );
                }
                if (keyframe.node !== undefined && keyframe.edge !== undefined) {
                    const options = { node: keyframe.node, edge: keyframe.edge };
                    this.#jar.updateCode(makeFGCode(options));
                    this.update(options);
                }
                if (keyframe.callback !== undefined) {
                    keyframe.callback();
                }
            }, (totalDelay += keyframe.delay === undefined ? 5 * 1000 : keyframe.delay));
        }
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

        const code = this.#jar.toString();
        if (this.#oldCode !== null && this.#oldCode === code) {
            return;
        }

        // if we're already waiting, don't kick off another request
        if (this.#currentlyWaitingForTiles) {
            this.#shouldRequestNewTilesWhenTilesAreLoaded = true;
            return;
        }

        this.#oldCode = code;
        const foo = Function('"use strict";return function(fg) { return ' + code + ' };');
        const bar = foo();
        bar(this.update);
    }

    handleTilesLoaded() {
        if (this.#shouldRequestNewTilesWhenTilesAreLoaded) {
            setTimeout(this.handleCodeChange, 0);
        }

        this.#curTileLayer.off('load', this.handleTilesLoaded);
        this.#curTileLayer.off('tileerror', this.handleTilesLoaded);
        if (this.#oldTileLayer !== null) {
            this.#map.removeLayer(this.#oldTileLayer);
            this.#oldTileLayer._map = {};
            this.#oldTileLayer = null;
        }

        this.#shouldRequestNewTilesWhenTilesAreLoaded = false;
        this.#currentlyWaitingForTiles = false;
    }

    clearMeasurements() {
        MeasuringTileLayer.clearMeasurements();
    }

    getMeasurements() {
        return MeasuringTileLayer.getMeasurements();
    }

    update(options) {
        options = {
            ...this.#options,
            ...options,
        };

        const newMeasurements = options.doMeasurements === undefined ? true : options.doMeasurements;
        if (newMeasurements !== this.#doMeasurements) {
            this.#doMeasurements = options.doMeasurements;
            if (this.#doMeasurements) {
                this.clearMeasurements();
            }
        }

        const url = fg(options);
        const constructor = this.#doMeasurements
            ? MeasuringTileLayer
            : L.TileLayer;
        const curTileLayer = new constructor(url, {
            tileSize: 256,
        });
        curTileLayer.addTo(this.#map);
        curTileLayer.bringToFront();

        this.#options = options;
        if (this.#curTileLayer !== null) {
            if (this.#oldTileLayer !== null) {
                this.#map.removeLayer(this.#oldTileLayer);
                this.#oldTileLayer._map = {};
                this.#oldTileLayer = null;
            }

            this.#oldTileLayer = this.#curTileLayer;
            this.#oldTileLayer.setUrl(L.Util.emptyImageUrl, true);
        }
        this.#curTileLayer = curTileLayer;
        curTileLayer.on('load', this.handleTilesLoaded);
        curTileLayer.on('tileerror', this.handleTilesLoaded);
    }
};

window.saveMeasurements = () => {
    const storage = window.localStorage;
    if (storage.getItem('n') === null) {
        storage.setItem('n', '0');
    }
    const n = +storage.getItem('n');
    storage.setItem(`${n}`, JSON.stringify(MeasuringTileLayer.getMeasurements()));
    storage.setItem('n', `${n+1}`);
    serializeMeasurements();
};

window.serializeMeasurements = () => {
    const storage = window.localStorage;
    const n = +storage.getItem('n');
    let ret = [];
    for (let i=0; i<n; ++i) {
        ret = [...ret, ...JSON.parse(storage.getItem(`${i}`))];
    }
    document.getElementById('measurements').value = JSON.stringify(ret);
    document.getElementById('measurements').style.display = 'block';
};

window.clearAllMeasurements = () => {
    window.localStorage.removeItem('n');
};