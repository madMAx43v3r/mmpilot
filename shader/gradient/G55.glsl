#version 310 es

layout(location = 0) out vec4 out0;   // (Y, Ix, Iy, w)

uniform sampler2D uTmp;               // RGBA16F from H55: (Y, Sx, Dx, w)

uniform vec2 uInvSize;                // (1/W, 1/H)

vec2 SD(vec2 uv) {
    return texture(uTmp, uv).yz;    // (Sx, Dx)
}

void main()
{
    vec2 uv = gl_FragCoord.xy * uInvSize;

    vec4 c  = texture(uTmp, uv);
    if(c.w == 0) {
        out0 = vec4(c.r, 0, 0, 0);
        return;
    }
    vec2 dy = vec2(0.0, uInvSize.y);

    // Fetch (Sx, Dx) at y-2..y+2
    vec2 m2 = SD(uv - 2.0 * dy);
    vec2 m1 = SD(uv - 1.0 * dy);
    vec2 p1 = SD(uv + 1.0 * dy);
    vec2 p2 = SD(uv + 2.0 * dy);

    // g = [1 4 6 4 1]
    // d = [-1 -2 0 2 1]

    // Ix = vertical smoothing (g) applied to Dx
    float Ix = (1.0 * m2.y + 4.0 * m1.y + 6.0 * c.z + 4.0 * p1.y + 1.0 * p2.y);

    // Iy = vertical derivative (d) applied to Sx
    float Iy = (-1.0 * m2.x -2.0 * m1.x + 2.0 * p1.x + 1.0 * p2.x);

    vec2 I = vec2(Ix, Iy) * (1.0 / 96.0) * c.w;

    out0 = vec4(c.r, I.x, I.y, c.w);
}
