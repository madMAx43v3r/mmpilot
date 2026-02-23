#version 310 es
precision highp float;

layout(location = 0) out vec2 outFlow;

uniform sampler2D uRef;         // (Y, Ix, Iy, w)
uniform sampler2D uImg;         // (Y, w)
uniform sampler2D uFlow;        // (dx, dy)

uniform vec2  uInvSize;
uniform float uDamping;         // damping, e.g. 1e-4
uniform float uMinDet;          // det threshold, 1e-8

void main() {
    vec2 p = gl_FragCoord.xy;

    vec2 pix = texelFetch(uImg, ivec2(p), 0).xy;

    if(pix.y <= 0.0) {
        outFlow = vec2(0);
        return;
    }
    vec2 flow = texelFetch(uFlow, ivec2(p), 0).xy;

    vec2 uv = (p + flow) * uInvSize;

    if(uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
        outFlow = flow;
        return;
    }
    vec4 ref = texture(uRef, uv);

    if(ref.w <= 0.0) {
        outFlow = flow;
        return;
    }
    float R = ref.x - pix.x;   // residual

    float Ix = ref.y;
    float Iy = ref.z;

    // H = [[A,B],[B,C]]
    float A = Ix * Ix;
    float B = Ix * Iy;
    float C = Iy * Iy;

    // b = [D,E]
    float D = Ix * R;
    float E = Iy * R;

    // Damped solve
    float a = A + uDamping;
    float c = C + uDamping;
    float det = a * c - B * B;

    if(det > uMinDet) {
        // inv(H)*b
        vec2 Hb = vec2(
             c * D - B * E,
            -B * D + a * E
        );
        // GN step: x -= inv(H)*b
        flow -= Hb / det;
    }
    outFlow = flow;
}
