#version 460 core
#include "common.glsl"

layout(binding = UNIT_MESH_INFO) uniform sampler2D meshInfo;     // r: view depth of the meshes
uniform bool peakDepth;         // as in tile_render.comp

in vec2 offset;
flat in vec4 conicOpacity;
flat in vec4 colorDepth;
flat in vec2 slope;

out vec4 fragColor;

// Gaussian falloff of one splat, with premultiplied alpha for back-to-front blending. Fragments behind a mesh are dropped.
void main() {
    float power = -0.5 * (conicOpacity.x * offset.x * offset.x + conicOpacity.z * offset.y * offset.y)
                - conicOpacity.y * offset.x * offset.y;
    float alpha = min(0.99, conicOpacity.w * exp(power));
    if (power > 0.0 || alpha < ALPHA_MIN) discard;
    float depth = colorDepth.a;
    if (peakDepth) depth = clamp(depth + dot(slope, offset), depth * (1.0 - PEAK_RANGE), depth * (1.0 + PEAK_RANGE));
    if (depth >= texelFetch(meshInfo, ivec2(gl_FragCoord.xy), 0).r) discard;
    fragColor = vec4(colorDepth.rgb * alpha, alpha);
}
