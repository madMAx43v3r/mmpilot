#version 310 es
precision highp float;

layout(location = 0) out float outMono;

uniform sampler2D uSrc;

uniform float uWeight;

in vec2 vUV;

void main() {
    vec2 c = texture(uSrc, vUV).xy;

    gl_FragDepth = c.y * uWeight;

    outMono = c.x;
}
