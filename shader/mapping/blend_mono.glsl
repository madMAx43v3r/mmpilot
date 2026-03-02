#version 310 es
precision highp float;

layout(location = 0) out vec2 outImg;

uniform sampler2D uSrc0;
uniform sampler2D uSrc1;

uniform float uWeight;

void main()
{
    ivec2 p  = ivec2(gl_FragCoord.xy);

    vec2 L = texelFetch(uSrc0, p, 0).xy;
    vec2 R = texelFetch(uSrc1, p, 0).xy;

    outImg = L * (1.0 - uWeight) + R * uWeight;

    // float lw = 0.5;
    // float rw = 0.5;
    // float lw2 = L.y * L.y;
    // float rw2 = R.y * R.y;
    // float w_sum = lw2 + rw2;

    // if(w_sum > 0.001) {
    //     lw = lw2 / w_sum;
    //     rw = rw2 / w_sum;
    // }
    // outImg = L * lw + R * rw;
}
