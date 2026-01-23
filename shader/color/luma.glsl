#version 310 es

layout(location = 0) out float out0;

uniform sampler2D uSrc;     // RGB

uniform vec2 uInvSize;

void main()
{
    vec2 p   = vec2(gl_FragCoord.xy);
    vec2 uv  = p * uInvSize;

    vec3 rgb = texture(uSrc, uv).rgb;       // already 0..1

    float Y  = dot(rgb, vec3(0.2126, 0.7152, 0.0722));

    out0 = Y;
}
