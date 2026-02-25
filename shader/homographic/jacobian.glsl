#version 310 es
precision highp float;

layout(location = 0) out vec4 outJ0;
layout(location = 1) out vec2 outJ1;
layout(location = 2) out vec2 outR;     // (R, w)

uniform sampler2D uRef;         // RGBA16F, (Y, Ix, Iy, w)
uniform sampler2D uImg;         // RG16F, (Y, w)

uniform vec2 uCenter;           // uImg (pixels)
uniform vec2 uInvSize;          // uImg (1/pixels)

uniform float uParams[6];      // p0..p7

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    float uN = uParams[0] * p.x + uParams[1] * p.y + uParams[2];
    float vN = uParams[3] * p.x + uParams[4] * p.y + uParams[5];

    vec2 q  = vec2(uN, vN) + uCenter;
    vec2 uv = q * uInvSize;

    outJ0 = vec4(0);
    outJ1 = vec2(0);
    outR  = vec2(0);

    if(uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return;
    }
    vec2 pix = texelFetch(uImg, ivec2(gl_FragCoord.xy), 0).xy;  // (Y, w)

    vec4 proj = texture(uRef, uv);   // (Y, Ix, Iy, w)

    float w = sqrt(pix.y * proj.w);
    if(w < 0.001) {
        return;
    }
    float R = (proj.x - pix.x) * w;

    outR = vec2(R, w);

    float Ix = proj.y * w;
    float Iy = proj.z * w;

    float J0 = Ix * p.x;
    float J1 = Ix * p.y;
    float J2 = Ix;
    float J3 = Iy * p.x;
    float J4 = Iy * p.y;
    float J5 = Iy;

    outJ0 = vec4(J0, J1, J2, J3);
    outJ1 = vec2(J4, J5);
}
