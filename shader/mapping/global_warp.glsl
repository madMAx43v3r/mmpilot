#version 310 es
precision highp float;

layout(location = 0) out vec4 outImg;

uniform sampler2D uSrc;

uniform vec2 uCenter;
uniform vec2 uInvSize;

uniform float uParams[6];      // p0..p5

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    float uN = uParams[0] * p.x + uParams[1] * p.y + uParams[2];
    float vN = uParams[3] * p.x + uParams[4] * p.y + uParams[5];

    vec2 q = vec2(uN, vN) + uCenter;

    vec2 uv = q * uInvSize;

    outImg = texture(uSrc, uv);
}
