#version 310 es
precision highp float;

uniform sampler2D uSrc;               // (Y)

layout(location = 0) out vec2 out0;   // (Y, w)

uniform vec2  uCenter;        // center in pixel coords
uniform float uRadiusSq;      // squared radius in pixels

void main()
{
    vec2 p = gl_FragCoord.xy;
    vec2 q = p - uCenter;

    float Y = texelFetch(uSrc, ivec2(p), 0).x;

    float r2 = dot(q, q);
    float w = max(1.0 - r2 / uRadiusSq, 0.0);

    out0 = vec2(Y, w);
}
