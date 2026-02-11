#version 310 es

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform ivec2 uSize;     // (W,H)
uniform vec4  uK;        // (fx, fy, cx, cy)

// Rotation matrix camera->world, row-major:
// r00 r01 r02
// r10 r11 r12
// r20 r21 r22
uniform mat3 uRcw;

uniform float uLimit;   // threshold for clamp to zero (1..0)

// Gravity in world
const vec3 g_w = vec3(0.0, 0.0, -1.0);

layout(r16f, binding = 0) uniform writeonly image2D uOut;

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uSize.x || p.y >= uSize.y) {
        return;
    }
    float fx = uK.x, fy = uK.y, cx = uK.z, cy = uK.w;

    // Pixel center coordinates
    float u = float(p.x) + 0.5;
    float v = float(p.y) + 0.5;

    // Normalized image plane coordinates
    float xn = (u - cx) / fx;
    float yn = (v - cy) / fy;

    // Ray in camera coordinates (rectified pinhole)
    vec3 rc = normalize(vec3(xn, yn, 1.0));

    // Rotate into world coordinates
    vec3 rw = uRcw * rc;

    // cos(theta) between ray and gravity
    float c = dot(rw, g_w);

    float w = (c < uLimit ? 0.0 : c);

    imageStore(uOut, p, w);
}
