#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;   // RGBA16F (Y, Sx, Dx, w)

uniform sampler2D uSrc;   // (Y, w)

uniform vec2 uInvSize;    // (1/W, 1/H)

float Y(vec2 uv) {
    return texture(uSrc, uv).x;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * uInvSize;
    vec2 dx = vec2(uInvSize.x, 0.0);

    // taps at x-3..x+3
    float m3 = Y(uv - 3.0 * dx);
    float m2 = Y(uv - 2.0 * dx);
    float m1 = Y(uv - 1.0 * dx);
    vec2  c  = texture(uSrc, uv).xy;   // (Y, w)
    float p1 = Y(uv + 1.0 * dx);
    float p2 = Y(uv + 2.0 * dx);
    float p3 = Y(uv + 3.0 * dx);

    // g7 = [1 6 15 20 15 6 1]
    float Sx =
          1.0  * m3
        + 6.0  * m2
        + 15.0 * m1
        + 20.0 * c.x
        + 15.0 * p1
        + 6.0  * p2
        + 1.0  * p3;

    // d7 = [-1 -4 -5 0 5 4 1]
    float Dx =
        (-1.0) * m3
      + (-4.0) * m2
      + (-5.0) * m1
      + ( 5.0) * p1
      + ( 4.0) * p2
      + ( 1.0) * p3;

    out0 = vec4(c.x, Sx, Dx, c.y);
}
