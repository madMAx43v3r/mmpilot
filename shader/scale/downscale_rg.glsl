#version 310 es
precision highp float;

uniform sampler2D uSrc;         // RG (e.g., RG16F)

layout(location = 0) out vec2 out0;

uniform vec2 uInvSrcSize;       // (1/W, 1/H)

void main()
{
    vec2 p = gl_FragCoord.xy;

    // Center in source
    vec2 c = p * 2.0 * uInvSrcSize;

    vec2 dx = vec2(uInvSrcSize.x, 0);
    vec2 dy = vec2(0, uInvSrcSize.y);

    // Fetch the 3x3 neighborhood around c
    vec2 p00 = texture(uSrc, c - dx - dy).xy;
    vec2 p10 = texture(uSrc, c      - dy).xy;
    vec2 p20 = texture(uSrc, c + dx - dy).xy;

    vec2 p01 = texture(uSrc, c - dx).xy;
    vec2 p11 = texture(uSrc, c).xy;
    vec2 p21 = texture(uSrc, c + dx).xy;

    vec2 p02 = texture(uSrc, c - dx + dy).xy;
    vec2 p12 = texture(uSrc, c      + dy).xy;
    vec2 p22 = texture(uSrc, c + dx + dy).xy;

    // 3x3 weights:
    //      1 2 1
    //      2 2 2
    //      1 2 1
    vec2 sum =  (p00 + 2.0 * p10 + p20)
        + 2.0 * (p01 + 1.0 * p11 + p21)
              + (p02 + 2.0 * p12 + p22);

    out0 = sum * (1.0 / 14.0);
}
