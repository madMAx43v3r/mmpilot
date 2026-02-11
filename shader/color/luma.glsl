#version 310 es
precision highp float;
precision highp sampler2D;

layout(location = 0) out vec2 outY;     // (Y, w)

uniform sampler2D uRGBA;     // RGB

uniform vec2 uInvSize;

void main()
{
    vec2 p   = vec2(gl_FragCoord.xy);
    vec2 uv  = p * uInvSize;

    vec3 rgb = texture(uRGBA, uv).rgb;       // already 0..1

    float Y  = dot(rgb, vec3(0.2126, 0.7152, 0.0722));

    outY = vec2(Y, 1.0);
}
