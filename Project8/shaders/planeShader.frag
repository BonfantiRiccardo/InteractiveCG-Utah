#version 410

in vec2 texCoord;
in vec3 fragPos;
in vec4 shadowCoord; // Shadow coordinates for shadow mapping

layout (location = 0) out vec4 fragColor;

uniform sampler2DShadow shadowMap; // Texture sampler for the shadow map
uniform vec3 spotLightDir; // Direction of the spotlight
uniform float spotLightCutoff; // Cutoff angle for the spotlight (in degrees)

uniform sampler2D normalMap; // Normal map for the plane
uniform mat3 mvNormal; // Normal matrix for transforming normals to view space

uniform vec3 lightPos; // Position of the light source
uniform vec3 K_A; // Ambient reflection coefficient
uniform vec3 K_D; // Diffuse reflection coefficient
uniform vec3 K_S; // Specular reflection coefficient
uniform float shininess; // Shininess factor for specular reflection

uniform bool isPerspective; // Flag to indicate if the projection is perspective or orthographic

const float ambientStrength = 0.6; // Ambient light strength

void main()
{
    // Sample the normal from the normal map and transform it to view space
    vec3 sampledNormal = texture(normalMap, texCoord).rgb * 2.0 - 1.0; // Transform from [0,1] to [-1,1]
    sampledNormal = normalize(sampledNormal);

    // Apply blin-phong shading model to gray plane with shadow mapping
    vec3 N = normalize(mvNormal * sampledNormal); // Normal vector in view space
    vec3 L = normalize(lightPos - fragPos); // Light direction vector
    vec3 V = normalize(-fragPos); // View direction vector (assuming the camera is at the origin)

    if (!isPerspective) V = vec3(0.0, 0.0, 1.0); // For orthographic projection, view direction is along the z-axis

    // Ambient component
    vec3 ambient = K_A * ambientStrength;

    // Diffuse component
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = K_D * diff;

    // Specular component
    vec3 specular = vec3(0.0);
    if (diff > 0.0) {
        vec3 H = normalize(L + V); // Halfway vector
        float spec = pow(max(dot(N, H), 0.0), shininess);
        specular = K_S * spec;
    }

    float lit = textureProj(shadowMap, shadowCoord); // Sample the shadow map using projective texturing
    // Check if the fragment is within the spotlight cone
    float inCone = smoothstep(spotLightCutoff, 1.0, dot(-L, normalize(spotLightDir)));

    // Apply shadow mapping before shading
    fragColor = vec4(ambient + inCone * lit * (diffuse + specular), 1.0); 
}