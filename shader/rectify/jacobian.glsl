#version 310 es
precision highp float;

layout(location = 0) out vec2 outJ;    // (dR/dK2, dR/dK4)

uniform sampler2D uSrcPos;      // (v.xy, r, rd) from virtual_cam.glsl
uniform sampler2D uSrcGrad;     // (Y, Ix, Iy, w)

uniform vec2  uInvSrcSize;      // (1/inW, 1/inH)
uniform float uF;               // fisheye scale in px

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec4 src = texelFetch(uSrcPos, p, 0);

    vec2 v = src.xy;
    vec2 q = (src.w * uF) * v;
    vec2 uv = vec2(0.5) + q * uInvSrcSize;
    
    vec2 I_xy = texture(uSrcGrad, uv).yz;   // (Ix, Iy) in src-pixel units

    float r  = src.z;
    float r2 = r * r;
    float r4 = r2 * r2;

    vec2 dq_dK2 = (uF * r2) * v;
    vec2 dq_dK4 = (uF * r4) * v;

    outJ = vec2(dot(I_xy, dq_dK2), dot(I_xy, dq_dK4));
}
