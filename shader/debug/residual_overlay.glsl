#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;

uniform sampler2D uImg;      // (Y, ...)
uniform sampler2D uRes;      // residual (R, w)

void main()
{
    ivec2 p  = ivec2(gl_FragCoord.xy);

    float Y = texelFetch(uImg, p, 0).x;
    vec4  R = texelFetch(uRes, p, 0);

    float Rn = abs(R.x);

    vec3 RGB = vec3(Y) * (1.0 - Rn) + vec3(Rn, 0, 0);

    out0 = vec4(RGB, R.y > 0.0 ? 1.0 : 0.0);
}
