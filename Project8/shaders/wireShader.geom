#version 410 core

layout(triangles) in;
layout(line_strip, max_vertices = 4) out;

// The goal is to emit the edges of the triangle as lines to create a wireframe effect.
void main() {
    for (int i = 0; i < 4; i++) {
        vec4 p = gl_in[i % 3].gl_Position;
        p.z -= 0.0005 * p.w; // Offset the z-value to avoid z-fighting
        gl_Position = p;
        EmitVertex();
    }
    EndPrimitive();
}