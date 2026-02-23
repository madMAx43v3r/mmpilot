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

    vec2 q = p + flow * uWeight;

    vec2 uv = q * uInvSize;

    if(uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
        outImg = vec4(0.0);
    } else {
        outImg = texture(uImg, uv);
    }
}
