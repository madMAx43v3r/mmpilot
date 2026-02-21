#version 310 es
precision highp float;

layout(location = 0) out vec4 outColor;

uniform sampler2D uSrc;         // fisheye source

uniform vec2  uInvSrcSize;      // (1/inW, 1/inH)

// Virtual pinhole camera
uniform vec2  uCenter;          // (outW/2, outH/2)
uniform vec2  uInvF;            // inverse focal length (1/pix)
uniform mat3  uRot;             // camera rotation

// Fisheye intrinsics
uniform float uF;               // fisheye scale in px
uniform float uK2;              // r^2 coeff on angle-radius
uniform float uK4;              // r^4 coeff on angle-radius

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    // Virtual camera ray direction
    vec3 dirV = normalize(vec3(p * uInvF, 1));

    // Rotate into fisheye camera coordinates
    vec3 dirF = normalize(uRot * dirV);

    // Angle from optical axis
    float xy = length(dirF.xy);
    float theta = atan(xy, dirF.z);

    // Unit direction on image plane
    vec2 v = normalize(dirF.xy);

    // Distort in "angle radius" space: r (radians)
    float r  = theta;           // equidistant
    // float r = 2.0 * sin(theta / 2.0);   // equisolid
    // float r = 2.0 * tan(theta / 2.0);   // stereographic
    float r2 = r * r;
    float r4 = r2 * r2;
    float rd = r * (1.0 + uK2 * r2 + uK4 * r4);

    // Reproject to fisheye
    vec2 uv = vec2(0.5) + ((uF * rd) * v) * uInvSrcSize;

    if(uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
        outColor = vec4(0);
        return;
    }
    outColor = texture(uSrc, uv);
}
