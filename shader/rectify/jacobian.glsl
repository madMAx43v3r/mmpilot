#version 310 es

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform ivec2 uSize;     // (W,H)
uniform vec4  uK;        // (fx, fy, cx, cy)

// Inputs
uniform sampler2D uRes;      // (R, w) to detect out of bounds
uniform sampler2D uUV;       // RG = warped UV in normalized [0,1] coords (ready for texture())
uniform sampler2D uGradRef;  // (Y, Ix, Iy, w) in rectified ref grid, per pixel
uniform sampler2D uGradMov;  // (Y, Ix, Iy, w) in rectified mov grid, per pixel

// Output: (dr/dk1, dr/dk2)
layout(rg16f, binding = 0) uniform writeonly image2D uOutJ;

// Multiplicative model sensitivity in PIXELS at rectified pixel coords "pix"
// pix is expected in the same pixel coordinate system as cx,cy (i.e., pixel centers like i+0.5).
void dk_mul(vec2 p, out vec2 d_k1, out vec2 d_k2)
{
    float fx = uK.x, fy = uK.y, cx = uK.z, cy = uK.w;

    float x = (p.x - cx) / fx;
    float y = (p.y - cy) / fy;

    float r2 = x*x + y*y;
    float r4 = r2 * r2;

    d_k1 = vec2(fx * x * r2, fy * y * r2); // (du/dk1, dv/dk1) in pixels
    d_k2 = vec2(fx * x * r4, fy * y * r4); // (du/dk2, dv/dk2) in pixels
}

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uSize.x || p.y >= uSize.y) {
        return;
    }
    float w = texelFetch(uRes, p, 0).y;

    if(w <= 0) {
        imageStore(uOutJ, p, vec4(0));      // out of bounds
        return;
    }

    // Ref pixel center (pixel coords)
    vec2 pref = vec2(p) + vec2(0.5, 0.5);

    // Warped UV (normalized, ready for texture lookup)
    vec2 uv = texelFetch(uUV, p, 0).rg;

    // Gradients: ref at p (integer), mov at uv (bilinear)
    vec2 gRef = texelFetch(uGradRef, p, 0).gb;
    vec2 gMov = textureLod(uGradMov, uv, 0).gb;

    // Convert warped UV to pixel coordinates for k-sensitivity math.
    vec2 qpix = uv * vec2(uSize);

    // Sensitivity wrt k at pref and at qpix (both images affected)
    vec2 duv_dk1_ref, duv_dk2_ref;
    vec2 duv_dk1_mov, duv_dk2_mov;
    dk_mul(pref, duv_dk1_ref, duv_dk2_ref);
    dk_mul(qpix, duv_dk1_mov, duv_dk2_mov);

    // dr/dk = gMov·d(q)/dk - gRef·d(p)/dk
    float J_k1 = dot(gMov, duv_dk1_mov) - dot(gRef, duv_dk1_ref);
    float J_k2 = dot(gMov, duv_dk2_mov) - dot(gRef, duv_dk2_ref);

    imageStore(uOutJ, p, vec4(J_k1, J_k2, 0, 0));
}
