#version 310 es
precision highp float;

layout(location = 0) out float outMono;
layout(location = 1) out float outWeight;

uniform sampler2D uSrc;

in vec2  vUV;
in float vHW;

void main() {
    vec2 uv = vUV / vHW;

    vec4 c = texture(uSrc, uv);

    outMono   = c.x;
    outWeight = c.y;
}
