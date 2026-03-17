#version 310 es
precision highp float;

layout(location = 0) in vec2 inPos;

uniform float uPointSize;

void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);
    gl_PointSize = uPointSize;   // radius * 2 (in pixels)
}