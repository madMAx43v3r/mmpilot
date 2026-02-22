#version 310 es
precision highp float;

layout(location = 0) out vec4 outJ0;   // (du/droll, dv/droll, du/dpitch, dv/dpitch)
layout(location = 1) out vec2 outJ1;   // (du/dyaw,  dv/dyaw)

uniform sampler2D uSrc;

uniform vec2  uInvSrcSize;
uniform vec2  uCenter;
uniform vec2  uInvF;

// R_WB^T from gyro (constant wrt RPY_cam)
uniform mat3  uRwbT;

// RPY_cam (roll, pitch, yaw) in radians (ZYX: Rz(yaw)*Ry(pitch)*Rx(roll))
uniform vec3  uRPYcam;

// Fisheye intrinsics
uniform float uF;
uniform float uK2;
uniform float uK4;

// 0 = equidistant, 1 = equisolid, 2 = stereographic
uniform int   uModel;

const float EPS = 1e-8;

// --- Rotation helpers (ZYX)
mat3 Rx(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c,   -s,
        0.0, s,    c
    );
}
mat3 Ry(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
         c,  0.0,  s,
        0.0, 1.0, 0.0,
        -s,  0.0,  c
    );
}
mat3 Rz(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
         c, -s, 0.0,
         s,  c, 0.0,
        0.0,0.0,1.0
    );
}

mat3 dRx(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
        0.0, 0.0, 0.0,
        0.0, -s,  -c,
        0.0,  c,  -s
    );
}
mat3 dRy(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
        -s,  0.0,  c,
        0.0, 0.0, 0.0,
        -c,  0.0, -s
    );
}
mat3 dRz(float a) {
    float c = cos(a), s = sin(a);
    return mat3(
        -s, -c, 0.0,
         c, -s, 0.0,
        0.0,0.0,0.0
    );
}

// r(theta) and dr/dtheta for the chosen radial model
void radial(float theta, out float r, out float dr_dtheta)
{
    if(uModel == 0) { // equidistant
        r = theta;
        dr_dtheta = 1.0;
    } else if(uModel == 1) { // equisolid
        float h = 0.5 * theta;
        r = 2.0 * sin(h);
        dr_dtheta = cos(h);
    } else { // stereographic
        float h = 0.5 * theta;
        float c = cos(h);
        r = 2.0 * tan(h);
        dr_dtheta = 1.0 / max(c*c, EPS); // sec^2(h)
    }
}

void main()
{
    vec2 p = gl_FragCoord.xy - uCenter;

    // Virtual camera ray direction (dirV)
    vec3 dirV = normalize(vec3(p * uInvF, 1));

    // A = R_WB^T * dirV  (constant wrt RPY_cam)
    vec3 A = uRwbT * dirV;

    // Build R_BC(RPY_cam) = Rz(yaw)*Ry(pitch)*Rx(roll)
    float roll  = uRPYcam.x;
    float pitch = uRPYcam.y;
    float yaw   = uRPYcam.z;

    mat3 Rz_ = Rz(yaw);
    mat3 Ry_ = Ry(pitch);
    mat3 Rx_ = Rx(roll);

    mat3 R_BC = Rz_ * Ry_ * Rx_;

    // w = R_BC * A, dirF = normalize(w)
    vec3 w = R_BC * A;
    float s = length(w);
    vec3 dirF = w / max(s, EPS);

    // --- d(dirF)/d(angle)
    // dw/droll = (Rz*Ry*dRx)*A, etc.
    vec3 dw_droll  = (Rz_ * Ry_ * dRx(roll)) * A;
    vec3 dw_dpitch = (Rz_ * dRy(pitch) * Rx_) * A;
    vec3 dw_dyaw   = (dRz(yaw) * Ry_ * Rx_) * A;

    // d normalize: d(dirF) = (I - dirF dirF^T) * dw / s
    // Implement projector application without mat3 construction:
    // P*dw = dw - dirF * dot(dirF, dw)
    vec3 Pdw_roll  = dw_droll  - dirF * dot(dirF, dw_droll);
    vec3 Pdw_pitch = dw_dpitch - dirF * dot(dirF, dw_dpitch);
    vec3 Pdw_yaw   = dw_dyaw   - dirF * dot(dirF, dw_dyaw);

    vec3 dF_droll  = Pdw_roll  / max(s, EPS);
    vec3 dF_dpitch = Pdw_pitch / max(s, EPS);
    vec3 dF_dyaw   = Pdw_yaw   / max(s, EPS);

    // mapping dirF -> uv
    float xy = length(dirF.xy);
    float rho = max(xy, EPS);
    float theta = atan(xy, dirF.z);

    vec2 v = dirF.xy / rho;

    float r, dr_dtheta;
    radial(theta, r, dr_dtheta);

    float r2 = r * r;
    float r4 = r2 * r2;

    // --- Jacobian duv/d(dirF) (2x3), then chain with d(dirF)/d(angle)
    float x = dirF.x, y = dirF.y, z = dirF.z;
    float D = x*x + y*y + z*z;

    // dtheta/dx,y,z  (theta = atan2(rho, z))
    float invD = 1.0 / max(D, EPS);
    float invRho = 1.0 / rho;

    float dtheta_dx = (z * invD) * (x * invRho);
    float dtheta_dy = (z * invD) * (y * invRho);
    float dtheta_dz = -(rho * invD);

    // d(rd)/d(r)
    float drd_dr = 1.0
        + 3.0 * uK2 * r2
        + 5.0 * uK4 * r4;
    float drd_dtheta = drd_dr * dr_dtheta;

    // dv/dx and dv/dy using (I - v v^T)/rho
    // Iv*ex = ex - v*(v.x), Iv*ey = ey - v*(v.y)
    vec2 dv_dx = (vec2(1, 0) - v * v.x) * invRho;
    vec2 dv_dy = (vec2(0, 1) - v * v.y) * invRho;

    // dq/dx,y,z
    vec2 dq_dx = uF * (v * (drd_dtheta * dtheta_dx) + rd * dv_dx);
    vec2 dq_dy = uF * (v * (drd_dtheta * dtheta_dy) + rd * dv_dy);
    vec2 dq_dz = uF * (v * (drd_dtheta * dtheta_dz));

    // Chain: dq/dangle  (pixel units)
    vec2 dq_droll  = dq_dx * dF_droll.x  + dq_dy * dF_droll.y  + dq_dz * dF_droll.z;
    vec2 dq_dpitch = dq_dx * dF_dpitch.x + dq_dy * dF_dpitch.y + dq_dz * dF_dpitch.z;
    vec2 dq_dyaw   = dq_dx * dF_dyaw.x   + dq_dy * dF_dyaw.y   + dq_dz * dF_dyaw.z;

    // Output Jacobian in pixel units
    outJ0 = vec4(dq_droll, dq_dpitch);
    outJ1 = dq_dyaw;
}
