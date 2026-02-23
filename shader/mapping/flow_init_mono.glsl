#version 310 es
precision highp float;

layout(location = 0) out vec2 outFlow;

uniform vec2 uCenter;

uniform float uParams[6];      // p0..p5

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    float uN = uParams[0] * p.x + uParams[1] * p.y + uParams[2];
    float vN = uParams[3] * p.x + uParams[4] * p.y + uParams[5];

    vec2 q = vec2(uN, vN);

    outFlow = q - p;
}
