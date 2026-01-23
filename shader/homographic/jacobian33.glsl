#version 310 es

layout(location = 0) out vec4 outJ0;
layout(location = 1) out vec4 outJ1;

uniform sampler2D uCurLin;     // RGBA16F, luma in .a

uniform vec2 uCurSize;
uniform vec2 uInvCurSize;

uniform float uParams[8];      // p0..p7

float sampY(vec2 uv) {
    return texture(uCurLin, uv).a;
}

void main()
{
    vec2 p = vec2(gl_FragCoord.xy);

    float p0 = uParams[0], p1 = uParams[1], p2 = uParams[2], p3 = uParams[3];
    float p4 = uParams[4], p5 = uParams[5], p6 = uParams[6], p7 = uParams[7];

    float uN = p0 * p.x + p1 * p.y + p2;
    float vN = p3 * p.x + p4 * p.y + p5;
    float d  = p6 * p.x + p7 * p.y + 1.0;

    vec2 wp = vec2(uN, vN) / d;

    float mask = 1.0;
    if (wp.x < 1.0 || wp.y < 1.0 || wp.x >= (uCurSize.x - 1.0) || wp.y >= (uCurSize.y - 1.0)) {
        mask = 0.0;
    }

    vec2 uvC = wp * uInvCurSize;

    vec2 dx = vec2(uInvCurSize.x, 0.0);
    vec2 dy = vec2(0.0, uInvCurSize.y);

    float a00 = sampY(uvC - dx - dy);
    float a10 = sampY(uvC      - dy);
    float a20 = sampY(uvC + dx - dy);

    float a01 = sampY(uvC - dx);
    float a11 = sampY(uvC);
    float a21 = sampY(uvC + dx);

    float a02 = sampY(uvC - dx + dy);
    float a12 = sampY(uvC      + dy);
    float a22 = sampY(uvC + dx + dy);

    float Ix = (a20 + 2.0 * a21 + a22) - (a00 + 2.0 * a01 + a02);
    float Iy = (a02 + 2.0 * a12 + a22) - (a00 + 2.0 * a10 + a20);

    Ix *= 0.125;
    Iy *= 0.125;

    float invD  = 1.0 / d;
    float invD2 = invD * invD;

    float du0 = p.x * invD;
    float du1 = p.y * invD;
    float du2 = invD;
    float du3 = 0.0, du4 = 0.0, du5 = 0.0;
    float du6 = -(uN * p.x) * invD2;
    float du7 = -(uN * p.y) * invD2;

    float dv0 = 0.0, dv1 = 0.0, dv2 = 0.0;
    float dv3 = p.x * invD;
    float dv4 = p.y * invD;
    float dv5 = invD;
    float dv6 = -(vN * p.x) * invD2;
    float dv7 = -(vN * p.y) * invD2;

    float J0 = (Ix * du0 + Iy * dv0) * mask;
    float J1 = (Ix * du1 + Iy * dv1) * mask;
    float J2 = (Ix * du2 + Iy * dv2) * mask;
    float J3 = (Ix * du3 + Iy * dv3) * mask;

    float J4 = (Ix * du4 + Iy * dv4) * mask;
    float J5 = (Ix * du5 + Iy * dv5) * mask;
    float J6 = (Ix * du6 + Iy * dv6) * mask;
    float J7 = (Ix * du7 + Iy * dv7) * mask;

    outJ0 = vec4(J0, J1, J2, J3);
    outJ1 = vec4(J4, J5, J6, J7);
}
