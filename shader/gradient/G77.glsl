#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;   // (Y, Ix, Iy, w)

uniform sampler2D uTmp;               // RGBA16F from H77: (Y, Sx, Dx, w)

uniform vec2 uInvSize;                // (1/W, 1/H)

vec2 SD(vec2 uv) {
    return texture(uTmp, uv).yz;      // (Sx, Dx)
}

void main()
{
    vec2 uv = gl_FragCoord.xy * uInvSize;

    vec4 c = texture(uTmp, uv);
    if(c.w <= 0.0) {
        out0 = vec4(c.x, 0, 0, 0);
        return;
    }
    vec2 dy = vec2(0, uInvSize.y);

    // Fetch (Sx, Dx) at y-3..y+3
    vec2 m3 = SD(uv - 3.0 * dy);
    vec2 m2 = SD(uv - 2.0 * dy);
    vec2 m1 = SD(uv - 1.0 * dy);
    vec2 p1 = SD(uv + 1.0 * dy);
    vec2 p2 = SD(uv + 2.0 * dy);
    vec2 p3 = SD(uv + 3.0 * dy);

    // g7 = [1 6 15 20 15 6 1]
    // d7 = [-1 -4 -5 0 5 4 1]

    // Ix = vertical smoothing (g7) applied to Dx
    float Ix =
          1.0  * m3.y
        + 6.0  * m2.y
        + 15.0 * m1.y
        + 20.0 * c.z
        + 15.0 * p1.y
        + 6.0  * p2.y
        + 1.0  * p3.y;

    // Iy = vertical derivative (d7) applied to Sx
    float Iy =
        (-1.0) * m3.x
      + (-4.0) * m2.x
      + (-5.0) * m1.x
      + ( 5.0) * p1.x
      + ( 4.0) * p2.x
      + ( 1.0) * p3.x;

    vec2 I = vec2(Ix, Iy) * (1.0 / 1024.0);

    out0 = vec4(c.x, I.x, I.y, c.w);
}
