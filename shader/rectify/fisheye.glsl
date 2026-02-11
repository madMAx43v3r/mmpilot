#version 310 es
precision highp float;
precision highp sampler2D;

layout(location = 0) out vec4 outColor;

uniform sampler2D uSrc;
uniform vec2 uSrcInvSize;

// patch -> ray
uniform vec2  uPatchC;      // center in patch pixels
uniform float uPatchInvF;   // inverse fx, fy in patch pixels
uniform mat3  uRot;         // virtual cam rotation into fisheye cam space

// fisheye intrinsics
uniform vec2  uFishC;      // cx, cy in src pixels
uniform float uFishF;      // f in pixels (or separate fx,fy if needed)

// pick one model
float rho_from_theta(float theta) {
    // equidistant:
    return uFishF * theta;

    // stereographic:
    // return 2.0 * uFishF * tan(0.5 * theta);

    // equisolid:
    // return 2.0 * uFishF * sin(0.5 * theta);
}

void main() {
    vec2 p = gl_FragCoord.xy; // patch pixel coords [0..patchW/H)

    vec2 n = (p - uPatchC) * uPatchInvF;
    vec3 r = normalize(vec3(n, 1));
    vec3 dir = normalize(uRot * r);

    outColor = vec4(0);

    // behind camera?
    if(dir.z <= 0.0) {
        return;
    }
    float theta = acos(clamp(dir.z, -1.0, 1.0));
    float phi   = atan(dir.y, dir.x);

    float rho = rho_from_theta(theta);

    vec2 srcPx = uFishC + rho * vec2(cos(phi), sin(phi));
    vec2 uv = srcPx * uSrcInvSize;

    // bounds check
    if(uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1) {
        return;
    }
    outColor = texture(uSrc, uv);
}
