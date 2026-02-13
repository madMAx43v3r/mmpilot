#version 310 es
precision highp float;

layout(location = 0) out vec4 outJ0;
layout(location = 1) out vec4 outJ1;
layout(location = 2) out vec2 outR;     // (R, w)
layout(location = 3) out vec2 outUV;    // (u, v)

uniform sampler2D uRef;         // RGBA16F, (Y, Ix, Iy, w)
uniform sampler2D uImg;         // RG16F, (Y, w)

uniform vec2 uCenter;           // uImg (pixels)
uniform vec2 uInvSize;          // uImg (1/pixels)

uniform float uParams[8];      // p0..p7

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;
    vec2 q = p * uInvSize;     // (x/W, y/H)

    float p0 = uParams[0], p1 = uParams[1], p2 = uParams[2], p3 = uParams[3];
    float p4 = uParams[4], p5 = uParams[5], p6 = uParams[6], p7 = uParams[7];

    float uN = p0 * p.x + p1 * p.y + p2;
    float vN = p3 * p.x + p4 * p.y + p5;
    float d  = p6 * q.x + p7 * q.y + 1.0;

    float invD = 1.0 / d;

    vec2 wp = vec2(uN, vN) * invD + uCenter;
    vec2 uv = wp * uInvSize;

    outJ0 = vec4(0);
    outJ1 = vec4(0);
    outR  = vec2(0);
    outUV = vec2(uv.x, uv.y);

    if(uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
        return;
    }
    vec2 pix = texelFetch(uImg, ivec2(gl_FragCoord.xy), 0).xy;  // (Y, w)

    vec4 proj = texture(uRef, uv);   // (Y, Ix, Iy, w)

    float w = sqrt(pix.y * proj.w);
    if(w <= 0.0) {
        return;
    }
    float R = (proj.x - pix.x) * w;

    outR = vec2(R, w);

    float Ix = proj.y * w;
    float Iy = proj.z * w;

    float invD2 = invD * invD;

    float du0 = p.x * invD;
    float du1 = p.y * invD;
    float du2 = invD;
    float du3 = 0.0, du4 = 0.0, du5 = 0.0;
    float du6 = -(uN * q.x) * invD2;
    float du7 = -(uN * q.y) * invD2;

    float dv0 = 0.0, dv1 = 0.0, dv2 = 0.0;
    float dv3 = p.x * invD;
    float dv4 = p.y * invD;
    float dv5 = invD;
    float dv6 = -(vN * q.x) * invD2;
    float dv7 = -(vN * q.y) * invD2;

    float J0 = (Ix * du0 + Iy * dv0);
    float J1 = (Ix * du1 + Iy * dv1);
    float J2 = (Ix * du2 + Iy * dv2);
    float J3 = (Ix * du3 + Iy * dv3);

    float J4 = (Ix * du4 + Iy * dv4);
    float J5 = (Ix * du5 + Iy * dv5);
    float J6 = (Ix * du6 + Iy * dv6);
    float J7 = (Ix * du7 + Iy * dv7);

    outJ0 = vec4(J0, J1, J2, J3);
    outJ1 = vec4(J4, J5, J6, J7);
}
