#version 310 es

out vec2 vUV;

void main() {
	// Fullscreen triangle (0,0), (2,0), (0,2) in clip space mapped via gl_VertexID
	vec2 p = vec2((gl_VertexID << 1) & 2, (gl_VertexID & 2));
	vUV = p;
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
