#version 310 es
precision highp float;

layout(location = 0) out vec4 outImg;

uniform sampler2D uImg;
uniform sampler2D uFlow;

uniform vec2  uInvSize;
uniform float uWeight;

void main() {
    vec2 p = vec2(gl_FragCoord.xy);

    vec2 flow = texelFetch(uFlow, ivec2(p), 0).xy;

    if(length(flow) < 0.01) {
        // lossless pass-through
        outImg = texelFetch(uImg, ivec2(p), 0);
        return;
    }
    vec2 q = p + flow * uWeight;

    vec2 uv = q * uInvSize;

    outImg = texture(uImg, uv);
}
