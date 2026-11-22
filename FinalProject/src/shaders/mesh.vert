#version 460 core
#define USE_MESH
#include "common.glsl"

uniform mat4 model;         // shape to world space, with a uniform scale
uniform mat4 view;
uniform mat4 projection;

out vec3  worldPosition;
out vec3  worldNormal;
out float viewDepth;

// One vertex of an object, read from the vertex buffer by its index.
void main() {
    MeshVertex vertex = meshVertex[gl_VertexID];
    vec4 world = model * vertex.position;
    vec4 inView = view * world;
    worldPosition = world.xyz;
    worldNormal = mat3(model) * vertex.normal.xyz;
    viewDepth = -inView.z;
    gl_Position = projection * inView;
}
