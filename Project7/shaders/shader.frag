#version 330


in vec3 fragNormal;
in vec3 fragPos;
in vec2 fragTexCoord;
in vec4 shadowCoord; // Shadow coordinates for shadow mapping

layout (location = 0) out vec4 fragColor;

uniform vec3 lightPos; // Position of the light source
uniform vec3 K_A; // Ambient reflection coefficient
uniform vec3 K_D; // Diffuse reflection coefficient
uniform vec3 K_S; // Specular reflection coefficient
uniform float shininess; // Shininess factor for specular reflection

uniform bool isPerspective; // Flag to indicate if the projection is perspective or orthographic

uniform sampler2D diffuseTexture; // Diffuse texture sampler
uniform sampler2D specularTexture; // Specular texture sampler
uniform sampler2D ambientTexture; // Ambient texture sampler

uniform sampler2DShadow shadowMap; // Texture sampler for the shadow map
uniform vec3 spotLightDir; // Direction of the spotlight
uniform float spotLightCutoff; // Cutoff angle for the spotlight (in degrees)

const float ambientStrength = 0.2; // Ambient light strength


void main()
{
    //Sample textures
    vec3 ambientTexColor = texture(ambientTexture, fragTexCoord).rgb; // Sample the ambient texture
    vec3 diffTexColor = texture(diffuseTexture, fragTexCoord).rgb; // Sample the diffuse texture
    vec3 specTexColor = texture(specularTexture, fragTexCoord).rgb; // Sample the specular texture


    //Prepare vectors for lighting calculations
    vec3 N = normalize(fragNormal); // Normal vector
    vec3 L = normalize(lightPos - fragPos); // Light direction vector
    vec3 V = normalize(-fragPos); // View direction vector (assuming the camera is at the origin)

    if (!isPerspective) V = vec3(0.0, 0.0, 1.0); // For orthographic projection, view direction is along the z-axis

    vec3 ambient = K_A * ambientStrength * ambientTexColor; // Ambient component

    // Diffuse component
    float diff = max(dot(normalize(N), normalize(L)), 0.0);
    vec3 diffuse = K_D * diff * diffTexColor;

    vec3 specular = vec3(0.0);

    float lit = textureProj(shadowMap, shadowCoord); // Sample the shadow map using projective texturing
    // Check if the fragment is within the spotlight cone
    float inCone = dot(-L, normalize(spotLightDir));
    inCone = inCone > spotLightCutoff ? 1.0 : 0.0; // Check if the fragment is within the spotlight cone

    // Specular component with half-vector method
    if (diff > 0.0) {
        vec3 H = normalize(L + V); // Halfway vector
        float spec = pow(max(dot(normalize(N), H), 0.0), shininess);
        specular = K_S * spec * specTexColor;
    }

    fragColor = vec4(ambient + inCone * lit * (diffuse + specular), 1.0);
}