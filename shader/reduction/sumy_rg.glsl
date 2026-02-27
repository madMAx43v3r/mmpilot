#version 310 es
precision highp float;

layout(location = 0) out vec2 out0;

uniform sampler2D uIn0;

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec2 sum0 = vec2(0);

    for (int i = 0; i < 1024; i++)      // compile-time cap
    {
        int y = i * uChunkSize + p.y;
        if (y >= uHeight) {
            break;
        }
        ivec2 t = ivec2(p.x, y);

        sum0 += texelFetch(uIn0, t, 0).xy;
    }

    out0 = sum0;
}