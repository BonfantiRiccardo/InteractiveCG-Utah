#version 330

in vec3 fragNormal;
in vec3 fragPos;
in vec4 shadowCoord; // Shadow coordinates for shadow mapping

layout (location = 0) out vec4 fragColor;

uniform sampler2DShadow shadowMap; // Texture sampler for the shadow map
uniform vec3 spotLightDir; // Direction of the spotlight
uniform float spotLightCutoff; // Cutoff angle for the spotlight (in degrees)

uniform vec3 lightPos; // Position of the light source
uniform vec3 K_A; // Ambient reflection coefficient
uniform vec3 K_D; // Diffuse reflection coefficient
uniform vec3 K_S; // Specular reflection coefficient
uniform float shininess; // Shininess factor for specular reflection

uniform bool isPerspective; // Flag to indicate if the projection is perspective or orthographic

const float ambientStrength = 0.6; // Ambient light strength

void main()
{
    // Apply blin-phong shading model to gray plane with shadow mapping
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPos); // Light direction vector
    vec3 V = normalize(-fragPos); // View direction vector (assuming the camera is at the origin)

    if (!isPerspective) V = vec3(0.0, 0.0, 1.0); // For orthographic projection, view direction is along the z-axis

    // Ambient component
    vec3 ambient = K_A * ambientStrength;

    // Diffuse component
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = K_D * diff;

    // Specular component
    vec3 H = normalize(L + V); // Halfway vector
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = K_S * spec;

    float lit = textureProj(shadowMap, shadowCoord); // Sample the shadow map using projective texturing
    // Check if the fragment is within the spotlight cone
    float inCone = dot(-L, normalize(spotLightDir));
    inCone = inCone > spotLightCutoff ? 1.0 : 0.0; // Check if the fragment is within the spotlight cone

    // Apply shadow mapping before shading
    fragColor = vec4(ambient + inCone * lit * (diffuse + specular), 1.0); 
}