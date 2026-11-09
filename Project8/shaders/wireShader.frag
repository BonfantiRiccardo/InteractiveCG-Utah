#version 410 core

layout (location = 0) out vec4 fragColor;

void main() {
    // Render a yellow wireframe color for the geometry
    fragColor = vec4(1.0, 1.0, 0.0, 1.0); // Yellow color
}