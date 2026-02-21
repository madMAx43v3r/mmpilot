#version 310 es
precision highp float;

layout(location = 0) out vec2  outMono;
layout(location = 1) out float outWeight;

uniform sampler2D uSrc;
uniform sampler2D uWeight;

in vec2 vUV;

void main() {
    vec2 c = texture(uSrc, vUV).xy;

    float w = texelFetch(uWeight, ivec2(gl_FragCoord.xy), 0).x;
    
    if(c.y <= w) {
        discard;
    }

    outMono   = c;
    outWeight = c.y;
}
