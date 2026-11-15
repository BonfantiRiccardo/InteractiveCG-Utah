#version 460 core

// One triangle that covers the whole render target.
void main() {
    gl_Position = vec4(vec2(gl_VertexID & 1, gl_VertexID >> 1) * 4.0 - 1.0, 0.0, 1.0);
}
