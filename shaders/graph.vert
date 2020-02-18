//#version 330 core
#version 130
#extension GL_ARB_explicit_attrib_location : require

precision mediump float;

layout(location = 0) in vec2 aPos;

void main(){
	gl_Position = vec4(aPos.x, aPos.y, 0.0f, 1.0f);
}
