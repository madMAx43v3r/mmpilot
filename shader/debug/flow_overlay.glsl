#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;

uniform sampler2D uImg;       // (Y, ...)
uniform sampler2D uFlow;      // flow (dx, dy)

uniform vec2 uCenter;

uniform float uParams[6];      // p0..p5

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    float uN = uParams[0] * p.x + uParams[1] * p.y + uParams[2];
    float vN = uParams[3] * p.x + uParams[4] * p.y + uParams[5];

    vec2 H = vec2(uN, vN) - p;

    vec2 Y = texelFetch(uImg, ivec2(gl_FragCoord.xy), 0).xy;
    vec2 F = texelFetch(uFlow, ivec2(gl_FragCoord.xy), 0).xy;

    F -= H;

    float g = min(length(F) / 2.0, 1.0);
    float r = min(length(F) / 8.0, 1.0);
    float b = min(length(F) / 32.0, 1.0);

    g = max(g - r, 0.0);
    r = max(r - b, 0.0);

    vec3 c = vec3(r, g, b) * 0.5;

    vec3 RGB = vec3(Y.x) * (1.0 - length(c)) + c;

    // out0 = vec4(RGB, 1.0);
    out0 = vec4(RGB, 1.0);
}
