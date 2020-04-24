//#version 330 core
#version 130
#extension GL_ARB_explicit_attrib_location : require

precision mediump float;

layout(location = 0) in float aPos;

uniform float uTranslateX;
uniform float uTranslateY;
uniform float uScale;

void main(){
	gl_Position = vec4(
		2. * (uScale * aPos.x + uTranslateX) - 1.,
		2. * (uScale * aPos.y + uTranslateY) - 1.,
		0.0f,
		1.0f
	);
}
