#version 310 es
precision highp float;

layout(location = 0) out vec2 outRW;

uniform sampler2D uSrc0;
uniform sampler2D uSrc1;

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    float R_sum = 0.0;
    float W_sum = 0.0;

    for (int i = 0; i < 1024; i++)      // compile-time cap
    {
        int y = i * uChunkSize + p.y;
        if (y >= uHeight) {
            break;
        }
        ivec2 t = ivec2(p.x, y);

        vec2 L = texelFetch(uSrc0, t, 0).xy;
        vec2 R = texelFetch(uSrc1, t, 0).xy;

        if(L.y * R.y > 0.0) {
            float r = R.x - L.x;
            R_sum += r * r;
            W_sum += 1.0;
        }
    }

    outRW = vec2(R_sum, W_sum);
}
