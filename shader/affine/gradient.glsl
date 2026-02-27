#version 310 es
precision highp float;

layout(location = 0) out vec4 outG;
layout(location = 1) out vec4 outH;
layout(location = 2) out vec4 outRWXY; // (R, w, H_xy)

uniform sampler2D uRes;  // residual texture (R, w)
uniform sampler2D uJ0;   // RGBA: J0..J3

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec4 G0 = vec4(0);
    vec4 H0 = vec4(0);

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

        G0 += J0123 * R.x;
        H0 += J0123 * J0123;
        H_xy += J0123.x * J0123.y;
    }

    outG = G0;
    outH = H0;
    outRWXY = vec4(R_sum, W_sum, H_xy, 1);
}
