#version 310 es
precision highp float;
precision highp sampler2D;

uniform sampler2D uSrc;         // R (e.g., R16F)

layout(location = 0) out vec4 out0;

uniform vec2 uInvSrcSize;       // (1/W, 1/H)

void main()
{
    vec2 p = gl_FragCoord.xy;

    // Center in source
    vec2 c = p * 2 * uInvSrcSize;

    vec2 dx = vec2(uInvSrcSize.x, 0);
    vec2 dy = vec2(0, uInvSrcSize.y);

    // Fetch the 3x3 neighborhood around c
    float p00 = texture(uSrc, c - dx - dy).x;
    float p10 = texture(uSrc, c      - dy).x;
    float p20 = texture(uSrc, c + dx - dy).x;

    float p01 = texture(uSrc, c - dx).x;
    float p11 = texture(uSrc, c).x;
    float p21 = texture(uSrc, c + dx).x;

    float p02 = texture(uSrc, c - dx + dy).x;
    float p12 = texture(uSrc, c      + dy).x;
    float p22 = texture(uSrc, c + dx + dy).x;

    // 3x3 Gaussian weights: 1 2 1 / 2 4 2 / 1 2 1, normalized by 1/16
    vec2 sum = (p00 + 2*p10 + p20) + 2 * (p01 + 2*p11 + p21) + (p02 + 2*p12 + p22);

    out0 = sum * (1.0 / 16.0);
}
