#version 310 es
precision highp float;

layout(location = 0) out vec4 out0;
layout(location = 1) out vec4 out1;
layout(location = 2) out vec4 out2;
layout(location = 3) out vec4 out3;

uniform sampler2D uJ0;   // RGBA: J0..J3
uniform sampler2D uJ1;   // RGBA: J4..J7

uniform int uHeight;     // of the *input* textures
uniform int uChunkSize;  // output rows (e.g., 16/32/64)

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    vec4 S0 = vec4(0.0);
    vec4 S1 = vec4(0.0);
    vec4 S2 = vec4(0.0);
    vec4 S3 = vec4(0.0);

    for (int i = 0; i < 1024; i++)      // compile-time cap
    {
        int y = i * uChunkSize + p.y;
        if (y >= uHeight) {
            break;
        }
        ivec2 t = ivec2(p.x, y);

        vec4 J0123 = texelFetch(uJ0, t, 0);
        vec4 J4567 = texelFetch(uJ1, t, 0);

        float J0 = J0123.x;
        float J1 = J0123.y;
        float J2 = J0123.z;
        float J3 = J0123.w;

        float J4 = J4567.x;
        float J5 = J4567.y;
        float J6 = J4567.z;
        float J7 = J4567.w;

        // MRT0: (0,1), (0,3), (0,4), (1,3)
        S0 += vec4(J0*J1, J0*J3, J0*J4, J1*J3);

        // MRT1: (1,4), (3,4), (0,2), (1,2)
        S1 += vec4(J1*J4, J3*J4, J0*J2, J1*J2);

        // MRT2: (3,5), (4,5), (2,5), (6,2)
        S2 += vec4(J3*J5, J4*J5, J2*J5, J6*J2);

        // MRT3: (6,5), (7,2), (7,5), (6,7)
        S3 += vec4(J6*J5, J7*J2, J7*J5, J6*J7);
    }

    out0 = S0;
    out1 = S1;
    out2 = S2;
    out3 = S3;
}
