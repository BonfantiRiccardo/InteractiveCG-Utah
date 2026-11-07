#version 330

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

out vec3 fragNormal;
out vec3 fragPos;
out vec4 shadowCoord; // Shadow coordinates for shadow mapping

uniform mat4 mvp;
uniform mat4 modelView;
uniform mat3 mvNormal;
uniform mat4 shadowMatrix; // Shadow matrix for transforming to light space

void main()
{
    gl_Position = mvp * vec4(position, 1.0);
    fragPos = vec3(modelView * vec4(position, 1.0));    // Transform the vertex position to eye space
    fragNormal = normalize(mvNormal * normal);           // Transform the vertex normal to eye space
    shadowCoord = shadowMatrix * vec4(position, 1.0);   // Transform the vertex position to light space for shadow mapping
}