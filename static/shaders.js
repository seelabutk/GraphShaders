
let vertexShader = `
#version 130

precision mediump float;

in float x;
in float y;
in float date;
in float nmaintainers;
in float cve;

uniform float uTranslateX;
uniform float uTranslateY;
uniform float uScale;

out float attr;

void main(){	
	
	attr = nmaintainers;	

	gl_Position = vec4(
		2. * (uScale * x + uTranslateX) - 1.,
		2. * (uScale * y + uTranslateY) - 1.,
		0.0f,
		1.0f
	);
}
`


let fragmentShader = `
#version 130
	
precision mediump float;

in float attr;

vec3 cmap[10];

cmap[0] = vec3(0.267004, 0.004874, 0.329415);
cmap[1] = vec3(0.268510, 0.009605, 0.335427);
cmap[2] = vec3(0.269944, 0.014625, 0.341379);
cmap[3] = vec3(0.271305, 0.019942, 0.347269);
cmap[4] = vec3(0.272594, 0.025563, 0.353093);
cmap[5] = vec3(0.273809, 0.031497, 0.358853);
cmap[6] = vec3(0.274952, 0.037752, 0.364543);
cmap[7] = vec3(0.276022, 0.044167, 0.370164);
cmap[8] = vec3(0.277018, 0.050344, 0.375715);
cmap[9] = vec3(0.277941, 0.056324, 0.381191);


vec3 apply_cmap(float data_attr){
	if(data_attr > 9.0f)
		data_attr = 9.0f;
	
	int index = int(floor(data_attr));
	return cmap[index];
}

void main(){
	float selection_color = 0;	//what is this?
	
	float alpha = 0.01f;
	vec3 temp_color = apply_cmap(attr);
	//temp_color.x = temp_color.x*(1.0 - selection_color);

	gl_FragColor = vec4(temp_color.xyz, alpha);
}
`
