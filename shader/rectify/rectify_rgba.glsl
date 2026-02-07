#version 310 es

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform sampler2D uSrc;      // distorted input (RGBA*)
uniform ivec2     uSize;     // (W, H)
uniform vec2      uInvSize;
uniform vec4      uK;        // (fx, fy, cx, cy)
uniform vec2      uRad;      // (k1, k2)  -> r^2, r^4

layout(rgba16f, binding = 0) uniform writeonly image2D uDst;

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uSize.x || p.y >= uSize.y) {
        return;
    }

    float fx = uK.x, fy = uK.y, cx = uK.z, cy = uK.w;

    float k1 = uRad.x;
    float k2 = uRad.y;

    // Undistorted pixel center
    float u = float(p.x) + 0.5;
    float v = float(p.y) + 0.5;

    // Normalized undistorted coordinates
    float xu = (u - cx) / fx;
    float yu = (v - cy) / fy;

    float r2 = xu*xu + yu*yu;
    float r4 = r2 * r2;

    float s = 1.0 + k1 * r2 + k2 * r4;

    // Distorted normalized coords
    float xd = xu * s;
    float yd = yu * s;

    // Back to distorted pixel coordinates
    float us = xd * fx + cx;
    float vs = yd * fy + cy;

    // Normalized texture coords for sampling
    vec2 uv = vec2(us, vs) * uInvSize;

    // Bilinear sample (explicit LOD for compute)
    vec4 rgba = textureLod(uSrc, uv, 0);

    imageStore(uDst, p, rgba);
}
