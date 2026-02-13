#version 310 es
precision highp float;

layout(location = 0) out vec2 out0;

uniform sampler2D uSrc;

vec2 F(ivec2 p, int x, int y)
{
    return texelFetch(uSrc, p + ivec2(x, y), 0).xy;
}

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec2 sum = vec2(0);

    //      1.0,  4.0,  6.0,  4.0, 1.0
    //      4.0, 16.0, 24.0, 16.0, 4.0
    //      6.0, 24.0, 36.0, 24.0, 6.0
    //      4.0, 16.0, 24.0, 16.0, 4.0
    //      1.0,  4.0,  6.0,  4.0, 1.0

    // Row -2
    // sum += F(p, -2,-2) * 1.0;
    // sum += F(p, -1,-2) * 4.0;
    sum += F(p,  0,-2) * 6.0;
    // sum += F(p,  1,-2) * 4.0;
    // sum += F(p,  2,-2) * 1.0;

    // Row -1
    // sum += F(p, -2,-1) * 4.0;
    sum += F(p, -1,-1) * 16.0;
    sum += F(p,  0,-1) * 24.0;
    sum += F(p,  1,-1) * 16.0;
    // sum += F(p,  2,-1) * 4.0;

    // Row 0
    sum += F(p, -2, 0) *  6.0;
    sum += F(p, -1, 0) * 24.0;
    sum += F(p,  0, 0) * 36.0;
    sum += F(p,  1, 0) * 24.0;
    sum += F(p,  2, 0) *  6.0;

    // Row +1
    // sum += F(p, -2, 1) * 4.0;
    sum += F(p, -1, 1) * 16.0;
    sum += F(p,  0, 1) * 24.0;
    sum += F(p,  1, 1) * 16.0;
    // sum += F(p,  2, 1) * 4.0;

    // Row +2
    // sum += F(p, -2, 2) * 1.0;
    // sum += F(p, -1, 2) * 4.0;
    sum += F(p,  0, 2) * 6.0;
    // sum += F(p,  1, 2) * 4.0;
    // sum += F(p,  2, 2) * 1.0;

    out0 = sum * (1.0 / (256.0 - 4.0 - 32.0));
}
