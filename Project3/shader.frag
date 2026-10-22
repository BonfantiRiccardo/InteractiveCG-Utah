#version 330


in vec3 fragNormal;
in vec3 fragPos;

layout (location = 0) out vec4 fragColor;

uniform vec3 lightPos; // Position of the light source
uniform vec3 K_A; // Ambient reflection coefficient
uniform vec3 K_D; // Diffuse reflection coefficient
uniform vec3 K_S; // Specular reflection coefficient
uniform float shininess; // Shininess factor for specular reflection

uniform bool isPerspective; // Flag to indicate if the projection is perspective or orthographic

void main()
{
    //Prepare vectors for lighting calculations
    vec3 N = normalize(fragNormal); // Normal vector
    vec3 L = normalize(lightPos - fragPos); // Light direction vector
    vec3 V = normalize(-fragPos); // View direction vector (assuming the camera is at the origin)

    if (!isPerspective) V = vec3(0.0, 0.0, 1.0); // For orthographic projection, view direction is along the z-axis

    vec3 ambient = K_A; // Ambient component

    // Diffuse component
    float diff = max(dot(normalize(N), normalize(L)), 0.0);
    vec3 diffuse = K_D * diff;

    vec3 specular = vec3(0.0);

    // Specular component with half-vector method
    if (diff > 0.0) {
        vec3 H = normalize(L + V); // Halfway vector
        float spec = pow(max(dot(normalize(N), H), 0.0), shininess);
        specular = K_S * spec;
    }

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}