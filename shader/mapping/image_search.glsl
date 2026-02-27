#version 310 es
precision highp float;

layout(location = 0) out vec2 outR;

uniform sampler2D uRef;
uniform sampler2D uImg;

uniform vec2 uCenter;
uniform vec2 uInvSize;
uniform int  uWidth;

uniform float uParams[4];      // p0, p1, p3, p4
uniform vec2  uOffset[32];

void main()
{
    vec2 p;
    p.x = mod(gl_FragCoord.x, float(uWidth));
    p.y =     gl_FragCoord.y;

    p -= uCenter;

    vec2 q;
    q.x = uParams[0] * p.x + uParams[1] * p.y;
    q.y = uParams[2] * p.x + uParams[3] * p.y;

    q += uCenter;

    int k = int(gl_FragCoord.x) / uWidth;

    vec2 uv = (q + uOffset[k]) * uInvSize;

    outR = vec2(0);

    if(uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return;
    }
    ivec2 t;
    t.x = int(gl_FragCoord.x) % uWidth;
    t.y = int(gl_FragCoord.y);

    vec2 pix = texelFetch(uImg, t, 0).xy;

    if(pix.y <= 0.0) {
        return;
    }
    vec2 ref = texture(uRef, uv).xy;

    if(ref.y <= 0.0) {
        return;
    }
    float w = sqrt(ref.y * pix.y);

    outR = vec2(abs(ref.x - pix.x) * w, w);
}
