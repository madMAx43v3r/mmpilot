#version 310 es
precision highp float;

uniform sampler2D uSrc;

layout(location = 0) out vec4 out0;

uniform vec2 uInvSrcSize;       // (1/W, 1/H)

void main()
{
    vec2 p = gl_FragCoord.xy;

    // Center in source
    vec2 c = p * (2.0 * uInvSrcSize);

    out0 = texture(uSrc, c);
}
