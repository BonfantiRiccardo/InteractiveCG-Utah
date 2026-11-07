#version 330

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

out vec3 fragNormal;
out vec3 fragPos;
out vec2 fragTexCoord;
out vec4 shadowCoord; // Coordinates for shadow mapping

uniform mat4 mvp;
uniform mat3 mvNormal;
uniform mat4 modelView;
uniform mat4 shadowMatrix; // Matrix to transform vertex positions to shadow map space


void main()
{
    gl_Position = mvp * vec4(position, 1.0);

    fragNormal = normalize(mvNormal * normal);          // Transform the normal to eye space and normalize it
    fragPos = vec3(modelView * vec4(position, 1.0));    // Transform the vertex position to eye space
    fragTexCoord = texCoord;                            // Pass the texture coordinates to the fragment shader
    shadowCoord = shadowMatrix * vec4(position, 1.0);   // Calculate the shadow coordinates for shadow mapping
}