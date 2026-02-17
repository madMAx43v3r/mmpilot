#version 310 es
precision highp float;

layout(location = 0) in vec2  inPos;    // map coords
layout(location = 1) in vec2  inUV;     // (0..1)
layout(location = 2) in float inHW;     // homography denom q.z

uniform vec2 uMapSize;              // (mapW, mapH)

out vec2  vUV;
out float vHW;

void main()
{
    // map pixels -> NDC
    vec2 ndc = (inPos / uMapSize) * 2.0 - 1.0;

    // Projective trick: make w = aW so interpolation is projective
    gl_Position = vec4(ndc * inHW, 0, inHW);

    vUV = inUV * inHW;
    vHW = inHW;
}
