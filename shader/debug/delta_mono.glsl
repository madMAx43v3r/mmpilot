#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;

uniform sampler2D uImg0;      // (Y, ...)
uniform sampler2D uImg1;      // (Y, ...)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec2 L = texelFetch(uImg0, p, 0).xy;
    vec2 R = texelFetch(uImg1, p, 0).xy;

    float Rn = abs(L.x - R.x);

    vec3 RGB = vec3(R.x) * (1.0 - Rn) + vec3(Rn, 0, 0);

    out0 = vec4(RGB, L.y * R.y > 0.0 ? 1.0 : 0.0);
}
