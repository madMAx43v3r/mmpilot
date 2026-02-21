#version 310 es
precision highp float;

layout(location = 0) out vec2  outMono;
layout(location = 1) out float outWeight;

uniform sampler2D uSrc;

in vec2 vUV;

void main() {
    vec2 c = texture(uSrc, vUV).xy;

    outMono   = vec2(c.x * c.y, c.y);
    outWeight = c.y;
}
