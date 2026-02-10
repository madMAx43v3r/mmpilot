#version 300 es
precision highp float;

in vec2 vUV;

uniform sampler2D uTex;

out vec4 oColor;

void main() {
    oColor = texture(uTex, vUV);
}
