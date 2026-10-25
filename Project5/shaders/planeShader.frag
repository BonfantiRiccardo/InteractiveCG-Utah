#version 330

in vec2 fragTexCoord;

layout (location = 0) out vec4 fragColor;

uniform sampler2D renderedTexture;

void main()
{
    fragColor = texture(renderedTexture, fragTexCoord) + vec4(0.1, 0.1, 0.1, 1.0); 
    // Add a small ambient term to avoid complete darkness
}