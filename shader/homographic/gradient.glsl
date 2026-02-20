#version 310 es
precision highp float;

layout(location = 0) out vec4 outG0;
layout(location = 1) out vec4 outD0;
layout(location = 2) out vec4 outGD;
layout(location = 3) out vec4 outRWXY; // (R, w, H_xy)

uniform sampler2D uRes;  // residual texture (R, w)
uniform sampler2D uJ0;   // RGBA: J0..J3
uniform sampler2D uJ1;   // RG: J4..J5

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec4 G0 = vec4(0);
    vec4 D0 = vec4(0);
    vec2 G1 = vec2(0);
    vec2 D1 = vec2(0);

    float R_sum = 0.0;
    float W_sum = 0.0;
    float H_xy = 0.0;

    for (int i = 0; i < 1024; i++)      // compile-time cap
    {
        int y = i * uChunkSize + p.y;
        if (y >= uHeight) {
            break;
        }
        ivec2 t = ivec2(p.x, y);

        vec2 R = texelFetch(uRes, t, 0).xy;

        R_sum += R.x * R.x;
        W_sum += R.y;

        vec4 J0123 = texelFetch(uJ0, t, 0);
        vec2 J45   = texelFetch(uJ1, t, 0).xy;

        G0 += J0123 * R.x;
        G1 += J45   * R.x;

        D0 += J0123 * J0123;
        D1 += J45 * J45;

        H_xy += J0123.z * J45.y;
    }

    outG0 = G0;
    outD0 = D0;
    outGD = vec4(G1, D1);

    outRWXY = vec4(R_sum, W_sum, H_xy, 1);
}
