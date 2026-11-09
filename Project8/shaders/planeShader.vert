#version 410

layout (location = 0) in vec3 position;

out vec2 texCoord; // Texture coordinates for the plane
out vec3 fragPos;
out vec4 shadowCoord; // Shadow coordinates for shadow mapping

uniform mat4 mvp;
uniform mat4 modelView;
uniform mat4 shadowMatrix; // Shadow matrix for transforming to light space

void main()
{
    gl_Position = mvp * vec4(position, 1.0);
    fragPos = vec3(modelView * vec4(position, 1.0));    // Transform the vertex position to eye space
    texCoord = position.xy * 0.5 + 0.5; // Plane quad spans -1,1 so this maps on 0,1 for texture coordinates
    shadowCoord = shadowMatrix * vec4(position, 1.0);   // Transform the vertex position to light space for shadow mapping
}