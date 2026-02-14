#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;

uniform sampler2D uSrc;      // input texture

uniform ivec2 uSize;         // texture size in pixels (W, H)

uniform int uFlipX;
uniform int uFlipY;

void main()
{
    ivec2 p  = ivec2(gl_FragCoord.xy);

    if(uFlipX > 0) p.x = (uSize.x - 1) - p.x;
    if(uFlipY > 0) p.y = (uSize.y - 1) - p.y;

    out0 = texelFetch(uSrc, p, 0);
}
