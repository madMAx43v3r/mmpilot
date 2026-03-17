#version 310 es
precision highp float;

uniform vec4 uColor;

out vec4 fragColor;

void main() {
    // gl_PointCoord is [0..1] inside the point sprite
    vec2 coord = gl_PointCoord - vec2(0.5);

    // discard outside circle (makes it round instead of square)
    if(length(coord) > 0.5) {
        discard;
    }
    fragColor = uColor;
}