#version 330 core

in vec3 direction; // Direction vector for the background quad

layout (location = 0) out vec4 fragColor;

uniform samplerCube envMap; // Cube map texture for the background

void main() {
    // Sample the cube map using the direction vector
    vec3 envColor = texture(envMap, vec3(direction.xy, -direction.z)).rgb;
    fragColor = vec4(envColor, 1.0);
}