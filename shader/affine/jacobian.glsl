#version 310 es
precision highp float;

layout(location = 0) out vec4 outJ;     // (x, y, alpha, scale)
layout(location = 1) out vec2 outR;     // (R, w)

uniform sampler2D uRef;         // RGBA16F, (Y, Ix, Iy, w)
uniform sampler2D uImg;         // RG16F, (Y, w)

uniform vec2 uCenter;           // uImg (pixels)
uniform vec2 uInvSize;          // uImg (1/pixels)

uniform float uParams[4];      // (x, y, alpha, scale)

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    float ca = cos(uParams[2]);
    float sa = sin(uParams[2]);

    vec2 q = uCenter + vec2(
        (ca * p.x - sa * p.y) * uParams[3] + uParams[0],
        (sa * p.x + ca * p.y) * uParams[3] + uParams[1]
    );
    vec2 uv = q * uInvSize;

    outJ = vec4(0);
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

    float du_da = (-sa * p.x - ca * p.y) * uParams[3];
    float dv_da = ( ca * p.x - sa * p.y) * uParams[3];

    float du_ds = (ca * p.x - sa * p.y);
    float dv_ds = (sa * p.x + ca * p.y);

    float J0 = Ix;
    float J1 = Iy;
    float J2 = Ix * du_da + Iy * dv_da;
    float J3 = Ix * du_ds + Iy * dv_ds;

    outJ = vec4(J0, J1, J2, J3);
}
