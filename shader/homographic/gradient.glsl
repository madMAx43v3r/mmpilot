#version 310 es
precision highp float;

layout(location = 0) out vec4 outG0; // b0..b3
layout(location = 1) out vec4 outG1; // b4..b7
layout(location = 2) out vec4 outD0; // d0..d3
layout(location = 3) out vec4 outD1; // d4..d7
layout(location = 4) out vec4 outRWXY; // (R, w, H_xy)

uniform sampler2D uRes;  // residual texture (R, w)
uniform sampler2D uJ0;   // RGBA: J0..J3
uniform sampler2D uJ1;   // RGBA: J4..J7

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec4 G0 = vec4(0);
    vec4 G1 = vec4(0);
    vec4 D0 = vec4(0);
    vec4 D1 = vec4(0);

    float R_sum = 0.0;
    float W_sum = 0.0;
    float H_xy = 0.0;
    float H_67 = 0.0;

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
        vec4 J4567 = texelFetch(uJ1, t, 0);

        G0 += J0123 * R.x;
        G1 += J4567 * R.x;

        D0 += J0123 * J0123;
        D1 += J4567 * J4567;

        H_xy += J0123.z * J4567.y;
        H_67 += J4567.z * J4567.w;
    }

    outG0 = G0;
    outG1 = G1;
    outD0 = D0;
    outD1 = D1;

    outRWXY = vec4(R_sum, W_sum, H_xy, H_67);
}
