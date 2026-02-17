#version 310 es
precision highp float;

layout(location = 0) out vec4 outMap;

uniform sampler2D uMap;
uniform sampler2D uWeight;

void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);

    float w = texelFetch(uWeight, p, 0).x;

    outMap = texelFetch(uMap, p, 0) / w;
}
