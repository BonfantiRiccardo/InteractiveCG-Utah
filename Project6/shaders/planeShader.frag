#version 330

in vec3 fragNormal;
in vec3 fragPos;

layout (location = 0) out vec4 fragColor;

uniform sampler2D reflectionTexture;
uniform samplerCube envMap;
uniform mat3 viewToWorld;
uniform vec2 screenSize;


void main()
{
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(-fragPos);
    vec3 R = viewToWorld * reflect(-V, N);
    
    vec3 envColor = texture(envMap, vec3(R.xy, -R.z)).rgb;


    vec4 refl = texture(reflectionTexture, gl_FragCoord.xy / screenSize);

    fragColor = vec4(mix(envColor, refl.rgb, refl.a) * 0.8, 1.0);

}