#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;

uniform sampler2D uSrc;

uniform vec2 uInvSize;

vec4 F(vec2 p, float dx, float dy)
{
    vec2 uv = (p + vec2(dx, dy)) * uInvSize;
    return texture(uSrc, uv);
}

void main()
{
    vec2 p = gl_FragCoord.xy;

    vec4 sum = vec4(0);

    //      1.0,  4.0,  6.0,  4.0, 1.0
    //      4.0, 16.0, 24.0, 16.0, 4.0
    //      6.0, 24.0, 36.0, 24.0, 6.0
    //      4.0, 16.0, 24.0, 16.0, 4.0
    //      1.0,  4.0,  6.0,  4.0, 1.0

    // Row -2
    // sum += F(p, -2,-2) * 1.0;
    sum += F(p, -1.0,-2.0) * 4.0;
    sum += F(p,  0.0,-2.0) * 6.0;
    sum += F(p,  1.0,-2.0) * 4.0;
    // sum += F(p,  2,-2) * 1.0;

    // Row -1
    sum += F(p, -2.0,-1.0) * 4.0;
    sum += F(p, -1.0,-1.0) * 16.0;
    sum += F(p,  0.0,-1.0) * 24.0;
    sum += F(p,  1.0,-1.0) * 16.0;
    sum += F(p,  2.0,-1.0) * 4.0;

    // Row 0
    sum += F(p, -2.0, 0.0) *  6.0;
    sum += F(p, -1.0, 0.0) * 24.0;
    sum += F(p,  0.0, 0.0) * 36.0;
    sum += F(p,  1.0, 0.0) * 24.0;
    sum += F(p,  2.0, 0.0) *  6.0;

    // Row +1
    sum += F(p, -2.0, 1.0) * 4.0;
    sum += F(p, -1.0, 1.0) * 16.0;
    sum += F(p,  0.0, 1.0) * 24.0;
    sum += F(p,  1.0, 1.0) * 16.0;
    sum += F(p,  2.0, 1.0) * 4.0;

    // Row +2
    // sum += F(p, -2, 2) * 1.0;
    sum += F(p, -1.0, 2.0) * 4.0;
    sum += F(p,  0.0, 2.0) * 6.0;
    sum += F(p,  1.0, 2.0) * 4.0;
    // sum += F(p,  2, 2) * 1.0;

    out0 = sum * (1.0 / (256.0 - 4.0));
}
