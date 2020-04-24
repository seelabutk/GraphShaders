const TILE_LAYER_URL = 'tile/{x}/{y}/{z}/256.png'

// Setup the transfer function editor
tfeditor = new setup_tf_editor(document.querySelector("#tfeditor"), function(tf){
	request_tiles();
});

tfeditors = []
tfeditors.push(tfeditor);

// setup the gradient slider 
let slider = document.getElementById('slider');

let slider_obj = noUiSlider.create(slider, {
    start: [0, 100],
    connect: true,
    range: {
        'min': -1000,
        'max': 1000
    },
});
slider_obj.on('change', function (values) {
    GRAD_MIN = parseFloat(values[0]) / 1000.0;
    GRAD_MAX = parseFloat(values[1]) / 1000.0;
    console.log(values);
    request_tiles();
});


FishEye = {};
FishEye.impact_pos = [0.0, 0.0];
FishEye.radius = 0.0;
FishEye.impact = 0.1;

let FISHEYE = `
    vec2 direction = vec2(vert.x - impact_pos.x, vert.y - impact_pos.y);
    direction = normalize(direction);
    float inside = float(int(distance < radius));
    pos = inside * impact * direction + vert;
`;

let NEIGHBORS = `
    selected = int(distance < radius);
`;

let FILTER_OUT = `
    alpha = selection_color * alpha;
`;

let GRAD_MIN = 0.0;
let GRAD_MAX = 1.0;

let vertex_shader = `
    #version 330
    precision highp float;

    in vec2 vert;
    in float attr1;

    uniform mat4 ortho;
    uniform mat4 lookat;

    //smooth out float keep;
    out float attr;

    flat out int selected;

    vec2 impact_pos = vec2(0.0, 0.0);
    float radius = 0.0;
    float impact = 0.0;

    void main() {
        attr = attr1;
        vec2 pos = vert;
        selected = 0;
        float distance = sqrt(pow(vert.x - impact_pos.x, 2) + pow(vert.y - impact_pos.y, 2));
        // FISHEYE
        // NEIGHBORS
        gl_Position = ortho * lookat * vec4(pos, 0.0, 1.0);
    }
`;

let fragment_shader = `
    #version 330
    precision highp float;

    in float attr;
    flat in int selected;
    out vec4 color;

    float tf[255];    
    uniform sampler2D tex;
    uniform vec3 cmap[256];

    float apply_tf(in float data_attr)
    {
        int index = int(floor(data_attr * 254));
        return 1.0 - 1.0 / (tf[index] + 1);
    }

    vec3 apply_cmap(in float data_attr)
    {
        int index = int(floor(data_attr * 255));
        return cmap[index];
    }

    void main() {
        float alpha = apply_tf(attr);
        float selection_color = float(selected);
        // FILTER_OUT
        vec3 temp_color = apply_cmap(attr);
        temp_color.x = temp_color.x * (1.0 - selection_color);

        //vec4 texcolor = texture(tex, gl_FragCoord.xy / 256.0);
        //vec2 gradient = vec2(dFdx(texcolor.r), dFdy(texcolor.r));
        //float grad_magnitude = length(gradient);
        //float grad_min = 0.0;
        //float grad_max = 1.0;
        // GRAD_THRESHOLD
        //int include = int(grad_magnitude > grad_min && grad_magnitude < grad_max);
        //color = vec4(temp_color, alpha * include);
        color = vec4(temp_color, alpha);

        //color = vec4(selection_color, selection_color, 1.0, alpha);
    }
`;

function process_tf(tf)
{
    tf = tf.map(function(x){
        return parseFloat((x / 10.0).toFixed(2));
    });
    return tf;
}

function sendEvent(type, tile, offset) {
    var radius = document.querySelector('.cursor').offsetWidth / 2.0;

    if (toggleFisheye == true || toggleNeighbors == true)
    {
        //console.log("Fisheye called", radius, tile, offset);
        let z = 2 ** tile.z; 
        let stride_x = z * 1024 / 256;
        let stride_y = z * 1024 / 256;
        offset.y = 256 - offset.y;
        let overall_x = tile.x * 256 + offset.x - (256 / 2.0);
        let overall_y = (stride_y - tile.y) * 256 + offset.y - (256 / 2.0);
        overall_x = overall_x / (z * 1024) * 2.0 - 1.0;
        overall_y = overall_y / (z * 1024) * 2.0 - 1.0;
        radius = radius / (z * 1024) * 2.0;

        FishEye.impact_pos = [overall_x, overall_y];
        FishEye.radius = radius;
        request_tiles();
    }
    else if (toggleSelection)
    {
        tf = process_tf(tfeditor.get_tf());
        return fetch('/event/', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                type,
                tile,
                offset,
                radius,
                tf
            }),
        }).then((response) => {
            return response.json();
        }).then((response) => {
            var data = response.results.join("\n");
            document.querySelector('.selected-list').innerText = data;
        });
    }
}

moveCursor = function(e)
{
    var cursor = document.querySelector(".cursor");
    var width = cursor.offsetWidth;
    var height = cursor.offsetHeight;
    var x = e.pageX - width / 2.0;
    var y = e.pageY - height / 2.0;
    cursor.style.left = x.toString() + "px";
    cursor.style.top= y.toString() + "px";
}

resizeCursor = function(e)
{
    var e = window.event || e;
    var delta = Math.max(-1, Math.min(1, (e.wheelDelta || -e.detail)));
    var cursor = document.querySelector(".cursor");
    var width = cursor.offsetWidth;
    console.log("Delta", delta);
    width += delta * 2;
    cursor.style.width = width.toString() + "px";
    cursor.style.height = cursor.style.width;
    moveCursor(e);
}

document.querySelector('body').addEventListener('mousemove', function(e){
    moveCursor(e);
});

document.querySelector('body').addEventListener('mousewheel', function(e){
    resizeCursor(e);
}, false);

L.GridLayer.StagehandInteraction = L.GridLayer.extend({
	createTile: function (coords) {
		var tile = document.createElement('div');
		//tile.innerHTML = [coords.x, coords.y, coords.z].join(', ');
		//tile.style.outline = '1px solid red';
		tile.style.pointerEvents = 'auto';
		const handler = this.mouseEventHandler

		tile.addEventListener('click', function(e) {
			handler.call(this, 'click', coords, e);
		}, false);

		tile.addEventListener('mousedown', function(e) {
			handler.call(this, 'mousedown', coords, e);
		}, false);

		tile.addEventListener('mousemove', function(e) {
			if (e.button)
            {
                handler.call(this, 'mousemove', coords, e);
            }
		}, false);

		tile.addEventListener('mouseup', function(e) {
			handler.call(this, 'mouseup', coords, e);
		}, false);

		return tile;
	},
	mouseEventHandler(type, coords, e) {
		const position = { x: e.clientX, y: e.clientY };
		const bounds = this.getBoundingClientRect();
		const offset = {
			x: position.x - bounds.left,
			y: position.y - bounds.top,
		};
		console.log(coords, offset, e.pageX, e.pageY);
		sendEvent(type, coords, offset);
	},
    pane: 'mapPane' 
});

L.gridLayer.stagehandInteraction = function(opts) {
    return new L.GridLayer.StagehandInteraction(opts);
};

const map = L.map('map', {
        crs: L.CRS.Simple,
        center: [0, 0],
        zoom: 1,
        fadeAnimation: false
});

let tiles = L.tileLayer(TILE_LAYER_URL, {
        minZoom: 0,
        tileSize: 256
}).addTo(map);
tile_layers = [];
tile_layers.push(tiles);

let toggleSelection = false;
L.easyButton('<i class="fas fa-crosshairs"></i>', (btn, map) => {
	if (toggleSelection) {
		map.removeLayer(interaction);
		map._handlers.forEach((handler) => handler.enable());
        btn.button.style.backgroundColor = "#FFFFFF";
        document.querySelector('.cursor').style.display = "none";
	} 
    else {
		map.addLayer(interaction);
		map._handlers.forEach((handler) => handler.disable());
        btn.button.style.backgroundColor = "#FFD536";
        document.querySelector('.cursor').style.display = "block";
	}
	toggleSelection = !toggleSelection;
}).addTo(map);

let toggleFisheye = false;
L.easyButton('<i class="fas fa-expand"></i>', (btn, map) => {
	if (toggleFisheye) {
		map.removeLayer(interaction);
		map._handlers.forEach((handler) => handler.enable());
        btn.button.style.backgroundColor = "#FFFFFF";
        document.querySelector('.cursor').style.display = "none";
	} 
    else {
		map.addLayer(interaction);
		map._handlers.forEach((handler) => handler.disable());
        btn.button.style.backgroundColor = "#FFD536";
        document.querySelector('.cursor').style.display = "block";
	}
	toggleFisheye = !toggleFisheye;
}).addTo(map);

let toggleNeighbors = false;
L.easyButton('<i class="fas fa-project-diagram"></i>', (btn, map) => {
	if (toggleNeighbors) {
		map.removeLayer(interaction);
		map._handlers.forEach((handler) => handler.enable());
        btn.button.style.backgroundColor = "#FFFFFF";
        document.querySelector('.cursor').style.display = "none";
	} 
    else {
		map.addLayer(interaction);
		map._handlers.forEach((handler) => handler.disable());
        btn.button.style.backgroundColor = "#FFD536";
        document.querySelector('.cursor').style.display = "block";
	}
	toggleNeighbors = !toggleNeighbors;
}).addTo(map);

/*
L.easyButton('<i class="fas fa-plus"></i>', (btn, map) => {
    let tiles = L.tileLayer(TILE_LAYER_URL, {
            minZoom: 0,
            tileSize: 256
    }).addTo(map);
    tile_layers.push(tiles);
}).addTo(map);
*/

const interaction = L.gridLayer.stagehandInteraction();

function revokeURL(){
    URL.revokeObjectURL(this.src);
}

request_tiles = function()
{
	tf = process_tf(tfeditor.get_tf());
    temp_fragment_shader = fragment_shader.replace("float tf[255]", 
            "float tf[255] = float[255](" + tf.join(",") + ")");

    //temp_fragment_shader = temp_fragment_shader.replace('// GRAD_THRESHOLD', 
    //        "grad_min=" + GRAD_MIN.toFixed(2) + ";\n" + "grad_max=" + GRAD_MAX.toFixed(2) + ";");

    var impact_pos_string = "vec2 impact_pos = vec2(" + FishEye.impact_pos[0].toString() + 
        ", " + FishEye.impact_pos[1].toString() + ");";
    temp_vertex_shader = vertex_shader.replace("vec2 impact_pos = vec2(0.0, 0.0);", 
            impact_pos_string);
    temp_vertex_shader = temp_vertex_shader.replace("float radius = 0.0;", 
            "float radius = " + FishEye.radius.toString() + ";");
    temp_vertex_shader = temp_vertex_shader.replace("float impact = 0.0;", 
            "float impact = " + FishEye.impact.toString() + ";");

    if (toggleFisheye) {
        temp_vertex_shader = temp_vertex_shader.replace("// FISHEYE", FISHEYE);
    }
    if (toggleNeighbors) {
        temp_vertex_shader = temp_vertex_shader.replace("// NEIGHBORS", NEIGHBORS);
        temp_fragment_shader = temp_fragment_shader.replace("// FILTER_OUT", FILTER_OUT);
    }

    for (var i in tiles._tiles)
    {
        var src = "/tile/" + i.replace(/:/g, '/') + "/256.png"
        var url = src + "?rand=" + Math.random().toString(36).substring(7);
        base64_vshader = btoa(temp_vertex_shader).replace("+", "%2B");
        base64_fshader = btoa(temp_fragment_shader).replace("+", "%2B");
        url += "&vshader=" + base64_vshader + "&fshader=" + base64_fshader;
        superagent
            .get(url)
            .set('Cache-Control', "no-cache")
            .responseType('blob')
            .then((response) => {
                var j = response.req.url
                    .match(/([-0-9]*)\/([-0-9]*)\/([-0-9]*)\/[0-9]*\.png/).slice(1, 4)
                    .map(function(d){return parseInt(d)}).join(":");
                tiles._tiles[j].el.onload = revokeURL;
                tiles._tiles[j].el.src = URL.createObjectURL(response.body);
            })
            .catch(err => {
                console.log(err)
            });
    }
}

/*
function poll()
{
    var new_time = new Date().getTime();
    if (new_time - time < 10000000)
    { 
        requestAnimationFrame(poll);
        return;
    }
    request_tiles();
    time = new_time;
    requestAnimationFrame(poll);
}
setTimeout(function(){
    time = new Date().getTime();
    requestAnimationFrame(poll)
}, 2000);
*/

