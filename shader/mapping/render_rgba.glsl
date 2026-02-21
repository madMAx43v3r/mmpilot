#version 310 es
precision highp float;

layout(location = 0) out vec4  outRGBA;
layout(location = 1) out float outWeight;

uniform sampler2D uSrc;

in vec2 vUV;

void main() {
    vec4 c = texture(uSrc, vUV);

    outRGBA   = vec4(c.rgb * c.w, c.w);
    outWeight = c.w;
}
