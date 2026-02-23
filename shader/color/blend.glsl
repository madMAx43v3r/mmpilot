#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;

uniform sampler2D uSrc0;
uniform sampler2D uSrc1;

uniform float uFactor;          // 0..1

void main()
{
    ivec2 p  = ivec2(gl_FragCoord.xy);

    vec4 L = texelFetch(uSrc0, p, 0);
    vec4 R = texelFetch(uSrc1, p, 0);

    out0 = L * (1.0 - uFactor) + R * uFactor;
}
