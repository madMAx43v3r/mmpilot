#version 310 es
precision highp float;

layout(location = 0) out vec2 out0;

uniform sampler2D uSrc0;
uniform sampler2D uSrc1;

void main()
{
    ivec2 p  = ivec2(gl_FragCoord.xy);

    vec2 L = texelFetch(uSrc0, p, 0).xy;
    vec2 R = texelFetch(uSrc1, p, 0).xy;

    float lw = 0.5;
    float rw = 0.5;
    float w_sum = L.y + R.y;

    if(w_sum > 0.001) {
        lw = L.y / w_sum;
        rw = R.y / w_sum;
    }
    out0 = L * lw + R * rw;
}
