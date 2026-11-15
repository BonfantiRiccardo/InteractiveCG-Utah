#version 460 core
#include "common.glsl"

in vec2 offset;
flat in vec4 conicOpacity;
flat in vec3 color;

out vec4 fragColor;

// Gaussian falloff of one splat, with premultiplied alpha for back-to-front blending.
void main() {
    float power = -0.5 * (conicOpacity.x * offset.x * offset.x + conicOpacity.z * offset.y * offset.y)
                - conicOpacity.y * offset.x * offset.y;
    float alpha = min(0.99, conicOpacity.w * exp(power));
    if (power > 0.0 || alpha < ALPHA_MIN) discard;
    fragColor = vec4(color * alpha, alpha);
}
