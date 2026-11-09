#version 410 core

// Specify primitive, spacing, and winding order for tessellation
layout(quads, equal_spacing, ccw) in;

uniform sampler2D displacementMap; // Displacement map for tessellation
uniform float displacementScale; // Scale factor for displacement

uniform float tessLevel; // Tessellation level for controlling the number of generated vertices

// SAME AS IN PLANE VERTEX SHADER
out vec2 texCoord; // Texture coordinates for the plane
out vec3 fragPos;
out vec4 shadowCoord; // Shadow coordinates for shadow mapping

uniform mat4 mvp;
uniform mat4 modelView;
uniform mat4 shadowMatrix; // Shadow matrix for transforming to light space

void main() {
    // Calculate the position of the vertex in world space using barycentric coordinates
    vec4 p = mix(mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x), 
             mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x), gl_TessCoord.y);

    // Calculate texture coordinates 
    texCoord = p.xy * 0.5 + 0.5; // Plane quad spans -1,1 so this maps on 0,1 for texture coordinates

    p.z += textureLod(displacementMap, texCoord, log2(float(textureSize(displacementMap, 0).x) / tessLevel)).r * displacementScale; // Apply displacement from the map

    //AS IN PLANE VERTEX SHADER
    gl_Position = mvp * p; // Transform the vertex position to clip space
    fragPos = vec3(modelView * p);    // Transform the vertex position to eye space
    shadowCoord = shadowMatrix * p;   // Transform the vertex position to light space for shadow mapping
}