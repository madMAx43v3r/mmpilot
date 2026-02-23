#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;

uniform sampler2D uSrc;

uniform vec2 uInvOutSize;       // (1/W, 1/H)

void main()
{
    vec2 p = gl_FragCoord.xy;

    vec2 uv = p * uInvOutSize;

    out0 = texture(uSrc, uv);
}
