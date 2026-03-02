#version 310 es
precision highp float;

layout(location = 0) out vec2 outImg;

uniform sampler2D uImg;
uniform sampler2D uFlow;

uniform vec2  uInvSize;

void main() {
    vec2 p = vec2(gl_FragCoord.xy);

    vec2 pix  = texelFetch(uImg, ivec2(p), 0).xy;
    vec2 flow = texelFetch(uFlow, ivec2(p), 0).xy;

    vec2 q = p + flow * (1.0 - pix.y);

    vec2 uv = q * uInvSize;

    outImg = texture(uImg, uv).xy;
}
