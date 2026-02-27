#version 310 es
precision highp float;

layout(location = 0) out vec2 out0;
layout(location = 1) out vec2 out1;
layout(location = 2) out vec2 out2;
layout(location = 3) out vec2 out3;
layout(location = 4) out vec2 out4;
layout(location = 5) out vec2 out5;
layout(location = 6) out vec2 out6;
layout(location = 7) out vec2 out7;

uniform sampler2D uIn0;
uniform sampler2D uIn1;
uniform sampler2D uIn2;
uniform sampler2D uIn3;
uniform sampler2D uIn4;
uniform sampler2D uIn5;
uniform sampler2D uIn6;
uniform sampler2D uIn7;

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec2 sum0 = vec2(0);
    vec2 sum1 = vec2(0);
    vec2 sum2 = vec2(0);
    vec2 sum3 = vec2(0);
    vec2 sum4 = vec2(0);
    vec2 sum5 = vec2(0);
    vec2 sum6 = vec2(0);
    vec2 sum7 = vec2(0);

    for (int i = 0; i < 1024; i++)      // compile-time cap
    {
        int y = i * uChunkSize + p.y;
        if (y >= uHeight) {
            break;
        }
        ivec2 t = ivec2(p.x, y);

        sum0 += texelFetch(uIn0, t, 0).xy;
        sum1 += texelFetch(uIn1, t, 0).xy;
        sum2 += texelFetch(uIn2, t, 0).xy;
        sum3 += texelFetch(uIn3, t, 0).xy;
        sum4 += texelFetch(uIn4, t, 0).xy;
        sum5 += texelFetch(uIn5, t, 0).xy;
        sum6 += texelFetch(uIn6, t, 0).xy;
        sum7 += texelFetch(uIn7, t, 0).xy;
    }

    out0 = sum0;
    out1 = sum1;
    out2 = sum2;
    out3 = sum3;
    out4 = sum4;
    out5 = sum5;
    out6 = sum6;
    out7 = sum7;
}