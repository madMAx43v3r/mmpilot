#version 310 es
precision highp float;

layout(location = 0) in vec2  inPos;    // map coords
layout(location = 1) in vec2  inUV;     // (0..1)

uniform vec2 uMapSize;              // (mapW, mapH)

out vec2  vUV;

void main()
{
    // map pixels -> NDC
    vec2 ndc = (inPos / uMapSize) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0, 1);

    vUV = inUV;
}
