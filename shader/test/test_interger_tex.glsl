#version 310 es
precision highp float;
precision highp int;
precision highp usampler2D;

layout(location=0) out vec4 out0;
uniform usampler2D uBayer;

void main() {
    uvec4 v = texelFetch(uBayer, ivec2(gl_FragCoord.xy), 0);
    out0 = vec4(float(v.r) / 65535.0, 0.0, 0.0, 1.0);
}
