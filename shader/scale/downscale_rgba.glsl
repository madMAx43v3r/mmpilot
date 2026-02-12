#version 310 es
precision highp float;

uniform sampler2D uSrc;         // RGBA (e.g., RGBA8, RGBA16F)

layout(location = 0) out vec4 out0;

uniform vec2 uInvSrcSize;       // (1/W, 1/H)

void main()
{
    vec2 p = gl_FragCoord.xy;

    // Center in source
    vec2 c = p * 2.0 * uInvSrcSize;

    vec2 dx = vec2(uInvSrcSize.x, 0);
    vec2 dy = vec2(0, uInvSrcSize.y);

    // Fetch the 3x3 neighborhood around c
    vec4 p00 = texture(uSrc, c - dx - dy);
    vec4 p10 = texture(uSrc, c      - dy);
    vec4 p20 = texture(uSrc, c + dx - dy);

    vec4 p01 = texture(uSrc, c - dx);
    vec4 p11 = texture(uSrc, c);
    vec4 p21 = texture(uSrc, c + dx);

    vec4 p02 = texture(uSrc, c - dx + dy);
    vec4 p12 = texture(uSrc, c      + dy);
    vec4 p22 = texture(uSrc, c + dx + dy);

    // 3x3 weights:
    //      1 2 1
    //      2 2 2
    //      1 2 1
    vec4 sum =  (p00 + 2.0 * p10 + p20)
        + 2.0 * (p01 + 1.0 * p11 + p21)
              + (p02 + 2.0 * p12 + p22);

    out0 = sum * (1.0 / 14.0);
}
