#version 310 es
precision highp float;

layout(location = 0) out vec4 outG0; // b0..b3
layout(location = 1) out vec4 outG1; // b4..b7
layout(location = 2) out vec4 outD0; // d0..d3
layout(location = 3) out vec4 outD1; // d4..d7

uniform sampler2D uRes;  // residual texture (R, R, R, mask)
uniform sampler2D uJ0;   // RGBA: J0..J3
uniform sampler2D uJ1;   // RGBA: J4..J7

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // rows per output pixel (e.g., 32/64, max 1024)
uniform int uChunkMask;  // bit mask for modulo

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    int x = p.x;
    int k = p.y & uChunkMask;

    // Initialize accumulators
    vec4 G0 = vec4(0.0);
    vec4 G1 = vec4(0.0);
    vec4 D0 = vec4(0.0);
    vec4 D1 = vec4(0.0);

    for (int i = 0; i < uChunkSize && i < 1024; i++)      // compile-time cap
    {
        int y = i * uChunkSize + k;
        if (y >= uHeight) {
            break;
        }
        ivec2 t = ivec2(x, y);

        float R = texelFetch(uRes, t, 0).r;    // rY (already masked is ideal)

        vec4 J0123 = texelFetch(uJ0, t, 0);
        vec4 J4567 = texelFetch(uJ1, t, 0);

        G0 += J0123 * R;
        G1 += J4567 * R;

        D0 += J0123 * J0123;
        D1 += J4567 * J4567;
    }

    outG0 = G0;
    outG1 = G1;
    outD0 = D0;
    outD1 = D1;
}
