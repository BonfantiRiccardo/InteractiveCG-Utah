#version 330

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

out vec3 fragNormal;
out vec3 fragPos;


uniform mat4 mvp;
uniform mat3 mvNormal;
uniform mat4 modelView;


void main()
{
    gl_Position = mvp * vec4(position, 1.0);

    fragNormal = normalize(mvNormal * normal);          // Transform the normal to eye space and normalize it
    fragPos = vec3(modelView * vec4(position, 1.0));    // Transform the vertex position to eye space
}