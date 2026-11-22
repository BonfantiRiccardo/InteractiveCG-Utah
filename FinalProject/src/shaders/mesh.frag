#version 460 core
#include "common.glsl"
#include "lighting.glsl"

uniform vec4 eye;               // camera position and 1, or for an orthographic camera the direction toward it and 0
uniform vec3 up;                // up direction of the scene
uniform vec3 color;             // albedo of a mesh, or color of the shape of a light
uniform bool unlit;             // shape of a light
uniform int  objectId;          // index of the object + 1
uniform int  debugView;

in vec3  worldPosition;
in vec3  worldNormal;
in float viewDepth;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragInfo;     // view depth, object id

const float SPECULAR = 0.5, SHININESS = 32.0;
const float GROUND_LIGHT = 0.35;            // light that arrives from below, relative to the sky

// Blinn-Phong shading in linear light: the sky term scaled by the ambient, plus each light of the scene.
void main() {
    vec3 n = normalize(worldNormal);
    vec3 v = normalize(eye.xyz - worldPosition * eye.w);
    if (dot(n, v) < 0.0) n = -n;
    vec3 result = vec3(1.0);
    if (unlit) result = color * (0.5 + 0.5 * dot(n, v));
    else if (debugView == VIEW_NORMALS) result = 0.5 + 0.5 * n;
    else if (debugView != VIEW_FLATNESS) {
        vec3 albedo = debugView == VIEW_LIGHT ? vec3(1.0) : color;
        vec3 sum = albedo * ambient * mix(GROUND_LIGHT, 1.0, 0.5 + 0.5 * dot(n, up));
        for (uint j = 0u; j < lightCount; j++) {
            vec3 l;
            vec3 E = irradiance(light[j], worldPosition, l);
            float diffuse = max(dot(n, l), 0.0);
            float highlight = diffuse > 0.0 ? pow(max(dot(n, normalize(l + v)), 0.0), SHININESS) : 0.0;
            sum += E * (albedo * diffuse + SPECULAR * highlight);
        }
        result = toSrgb(sum);
    }
    fragColor = vec4(result, 1.0);
    fragInfo = vec2(viewDepth, objectId);
}
