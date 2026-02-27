#version 310 es
precision highp float;

layout(location = 0) out vec4 outImg;

uniform sampler2D uSrc;

uniform vec2 uCenter;
uniform vec2 uInvSize;

uniform float uParams[4];      // affine

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    float ca = cos(uParams[2]);
    float sa = sin(uParams[2]);

    vec2 q = uCenter + vec2(
        (ca * p.x - sa * p.y) * uParams[3] + uParams[0],
        (sa * p.x + ca * p.y) * uParams[3] + uParams[1]
    );

    vec2 uv = q * uInvSize;

    outImg = texture(uSrc, uv);
}
