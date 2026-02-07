#version 310 es

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform sampler2D uSrc;         // RGBA (e.g., RGBA8, RGBA16F)

uniform ivec2     uDstSize;     // (W, H)
uniform vec2      uInvSrcSize;  // (1/W, 1/H)

layout(rgba16f, binding = 0) uniform writeonly image2D uDst; 

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);

    if (p.x >= uDstSize.x || p.y >= uDstSize.y) {
        return;
    }

    // Center in source
    vec2 c = (vec2(p) * 2 + vec2(1, 1)) * uInvSrcSize;

    vec2 dx = vec2(uInvSrcSize.x, 0);
    vec2 dy = vec2(0, uInvSrcSize.y);

    // Fetch the 3x3 neighborhood around c
    vec4 p00 = textureLod(uSrc, c - dx - dy, 0);
    vec4 p10 = textureLod(uSrc, c      - dy, 0);
    vec4 p20 = textureLod(uSrc, c + dx - dy, 0);

    vec4 p01 = textureLod(uSrc, c - dx, 0);
    vec4 p11 = textureLod(uSrc, c, 0);
    vec4 p21 = textureLod(uSrc, c + dx, 0);

    vec4 p02 = textureLod(uSrc, c - dx + dy, 0);
    vec4 p12 = textureLod(uSrc, c      + dy, 0);
    vec4 p22 = textureLod(uSrc, c + dx + dy, 0);

    // 3x3 Gaussian weights: 1 2 1 / 2 4 2 / 1 2 1, normalized by 1/16
    vec4 sum = (p00 + 2*p10 + p20) + 2 * (p01 + 2*p11 + p21) + (p02 + 2*p12 + p22);

    vec4 outV = sum * (1.0 / 16.0);

    imageStore(uDst, p, outV);
}
