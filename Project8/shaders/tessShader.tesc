#version 410 core

layout(vertices = 4) out;

uniform float tessLevel;

void main() {
    // Pass through the vertex positions to the next stage
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    // For each patch, set the tessellation levels. Only the first invocation needs to set these values.
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = tessLevel;
        gl_TessLevelOuter[1] = tessLevel;
        gl_TessLevelOuter[2] = tessLevel;
        gl_TessLevelOuter[3] = tessLevel;
        gl_TessLevelInner[0] = tessLevel;
        gl_TessLevelInner[1] = tessLevel;
    }
}