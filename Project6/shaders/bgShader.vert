#version 330

layout(location = 0) in vec3 position;

uniform mat4 inverseViewProjectionMatrix;

out vec3 direction;

void main()
{
    gl_Position = vec4(position.xy, 0.0, 1.0);
    direction = (inverseViewProjectionMatrix * vec4(position.xy, 1.0, 1.0)).xyz;
}
