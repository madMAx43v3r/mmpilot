#version 310 es
precision highp float;

layout(location = 0) out vec4 outGD;

uniform sampler2D uRes;  // residual texture (R, w)
uniform sampler2D uJ0;   // reference image (from jacobian.glsl)
uniform sampler2D uJ1;   // current image (from jacobian.glsl)

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec2 G0 = vec2(0);
    vec2 D0 = vec2(0);

    for (int i = 0; i < 1024; i++)      // compile-time cap
    {
        int y = i * uChunkSize + p.y;
        if (y >= uHeight) {
            break;
        }
        ivec2 t = ivec2(p.x, y);

        vec2 R = texelFetch(uRes, t, 0).xy;

        float w = R.y;
        if(w <= 0.0) {
            continue;
        }
        vec2 J0 = texelFetch(uJ0, t, 0).xy;
        vec2 J1 = texelFetch(uJ1, t, 0).xy;

        vec2 J01 = (J1 - J0) * w;

        G0 += J01 * R.x;
        D0 += J01 * J01;
    }

    outGD = vec4(G0, D0);
}
