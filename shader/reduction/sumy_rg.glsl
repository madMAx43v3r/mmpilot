#version 310 es
precision highp float;

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;

uniform sampler2D uInput;   // RG

uniform ivec2 uSize;      // input (W, H)

layout(rg32f, binding = 0) uniform writeonly image2D uOut;

void main()
{
    int x = gl_GlobalInvocationID.x;
    if(x >= uSize.x) {
        return;
    }

    vec2 sum = vec2(0);
    for(int y = 0; y < uSize.y && y < 1024; ++y) {
        sum += texelFetch(uInput, ivec2(x, y), 0).rg;
    }
    imageStore(uOut, ivec2(x, 0), sum);
}