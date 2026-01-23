#version 310 es

layout(location = 0) out vec4 out0;

uniform sampler2D uRefLin;     // R16F
uniform sampler2D uCurLin;     // R16F

uniform vec2 uCurSize;
uniform vec2 uInvRefSize;
uniform vec2 uInvCurSize;

uniform float uParams[8];      // homography p0..p7

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
    if (wp.x < 0.0 || wp.y < 0.0 || wp.x >= uCurSize.x || wp.y >= uCurSize.y) {
        mask = 0.0;
    }

    vec2 uv0 = p  * uInvRefSize;
    vec2 uv1 = wp * uInvCurSize;

    vec4 ref = texture(uRefLin, uv0);
    vec4 cur = texture(uCurLin, uv1);

    out0 = (cur - ref) * mask;
}
