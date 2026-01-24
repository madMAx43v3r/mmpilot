#version 310 es

layout(location = 0) out vec4 outJ0;
layout(location = 1) out vec4 outJ1;
layout(location = 2) out vec4 outR;

uniform sampler2D uRef;         // R16F
uniform sampler2D uGrad;        // RGB16F, (Y, Ix, Iy), zero border

uniform vec2 uInvSize;

uniform float uParams[8];      // p0..p7

void main()
{
    vec2 p = vec2(gl_FragCoord.xy);

    float p0 = uParams[0], p1 = uParams[1], p2 = uParams[2], p3 = uParams[3];
    float p4 = uParams[4], p5 = uParams[5], p6 = uParams[6], p7 = uParams[7];

    float uN = p0 * p.x + p1 * p.y + p2;
    float vN = p3 * p.x + p4 * p.y + p5;
    float d  = p6 * p.x + p7 * p.y + 1.0;

    vec2 wp = vec2(uN, vN) / d;
    vec2 uv = wp * uInvSize;

    if (uv.x < 0 || uv.y < 0 || uv.x > 1.0 || uv.y > 1.0) {
        outJ0 = vec4(0.0);
        outJ1 = vec4(0.0);
        outR = vec4(0.0);
        return;
    }

    vec3 grad = texture(uGrad, uv).rgb;  // (Y, Ix, Iy)

    float Y  = grad.r;
    float Ix = grad.g;
    float Iy = grad.b;

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

    // TODO: move to gradient shader
    float Yref = texture(uRef, p * uInvSize).r;
    float R = Y - Yref;

    outR = vec4(vec3(R), 1.0);
}
