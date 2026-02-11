#version 310 es
precision highp float;
precision highp int;
precision highp usampler2D;

layout(location = 0) out vec2 outY;

uniform usampler2D uBayer;   // GL_R16UI texture, values 0..65535

float f16(ivec2 p)
{
    uint v = texelFetch(uBayer, p, 0).r;
    return float(v) * (1.0 / 65535.0);
}

void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);

    // BGGR pattern:
    // y even, x even : B
    // y even, x odd  : G (on B row)
    // y odd,  x even : G (on R row)
    // y odd,  x odd  : R
    bool xOdd = (p.x & 1) != 0;
    bool yOdd = (p.y & 1) != 0;

    ivec2 size = textureSize(uBayer, 0);

    // Clamp neighbors to valid range (texelFetch out-of-bounds is undefined)
    ivec2 pL  = ivec2(max(p.x - 1, 0), p.y);
    ivec2 pR  = ivec2(min(p.x + 1, size.x - 1), p.y);
    ivec2 pU  = ivec2(p.x, max(p.y - 1, 0));
    ivec2 pD  = ivec2(p.x, min(p.y + 1, size.y - 1));
    ivec2 pUL = ivec2(pL.x, pU.y);
    ivec2 pUR = ivec2(pR.x, pU.y);
    ivec2 pDL = ivec2(pL.x, pD.y);
    ivec2 pDR = ivec2(pR.x, pD.y);

    float C  = f16(p);
    float L  = f16(pL);
    float Rt = f16(pR);
    float U  = f16(pU);
    float D  = f16(pD);
    float UL = f16(pUL);
    float UR = f16(pUR);
    float DL = f16(pDL);
    float DR = f16(pDR);

    float R = 0.0, G = 0.0, B = 0.0;

    // BGGR:
    // (even,even)=B, (even,odd)=G, (odd,even)=G, (odd,odd)=R
    if(!yOdd && !xOdd) {
        // B pixel
        B = C;
        G = 0.25 * (L + Rt + U + D);
        R = 0.25 * (UL + UR + DL + DR);
    } else if(!yOdd && xOdd) {
        // G pixel on B row
        G = C;
        B = 0.5 * (L + Rt);
        R = 0.5 * (U + D);
    } else if(yOdd && !xOdd) {
        // G pixel on R row
        G = C;
        R = 0.5 * (L + Rt);
        B = 0.5 * (U + D);
    } else {
        // R pixel
        R = C;
        G = 0.25 * (L + Rt + U + D);
        B = 0.25 * (UL + UR + DL + DR);
    }

    // Rec.709 luma
    float Y = dot(vec3(R, G, B), vec3(0.2126, 0.7152, 0.0722));
    outY = vec2(Y, 1);
}
