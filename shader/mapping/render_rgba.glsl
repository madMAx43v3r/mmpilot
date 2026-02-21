#version 310 es
precision highp float;

layout(location = 0) out vec4  outRGBA;
layout(location = 1) out float outWeight;

uniform sampler2D uSrc;
uniform sampler2D uWeight;

in vec2 vUV;

void main() {
    vec4 c = texture(uSrc, vUV);

    float w = texelFetch(uWeight, ivec2(gl_FragCoord.xy), 0).x;
    
    if(c.w <= w) {
        discard;
    }

    outRGBA   = c;
    outWeight = c.w;
}
