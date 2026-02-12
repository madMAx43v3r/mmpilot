#version 310 es
precision mediump float;

layout(location = 0) out vec4 out0;   // RGBA16F (Y, Sx, Dx, w)

uniform sampler2D uSrc;            // (Y, w)

uniform vec2 uInvSize;             // (1/W, 1/H)

float Y(vec2 uv) {
    return texture(uSrc, uv).x;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * uInvSize;
    vec2 dx = vec2(uInvSize.x, 0.0);

    // taps at x-2..x+2
    float m2 = Y(uv - 2.0 * dx);
    float m1 = Y(uv - 1.0 * dx);
    vec2  c  = texture(uSrc, uv).xy;
    float p1 = Y(uv + 1.0 * dx);
    float p2 = Y(uv + 2.0 * dx);

    // g = [1 4 6 4 1]
    float Sx = (1.0 * m2 + 4.0 * m1 + 6.0 * c.x + 4.0 * p1 + 1.0 * p2);

    // d = [-1 -2 0 2 1]
    float Dx = (-1.0 * m2 - 2.0 * m1 + 2.0 * p1 + 1.0 * p2);

    out0 = vec4(c.x, Sx, Dx, c.y);
}
