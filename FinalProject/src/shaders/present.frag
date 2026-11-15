#version 460 core
#include "common.glsl"

uniform sampler2D image;        // output of the active renderer
uniform sampler2D otherImage;   // output of the other renderer
uniform int debugView;          // 0: image, 1: splats per tile, 2: difference between the renderers
uniform int tilesX;

out vec4 fragColor;

const float HEAT_MAX = 4000.0;  // splats per tile shown as red

// Color scale from blue (0) through green to red (1).
vec3 heat(float t) {
    return clamp(1.5 - abs(4.0 * t - vec3(3.0, 2.0, 1.0)), 0.0, 1.0);
}

// Final image of the frame, with the selected debug view.
void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 color = clamp(texelFetch(image, pixel, 0).rgb, 0.0, 1.0);
    if (debugView == 1) {           // if debug is "splats per tile" show heat map
        ivec2 tile = pixel / TILE_SIZE;
        uvec2 range = tileRange[tile.y * tilesX + tile.x];
        color = mix(color, heat(min(float(range.y - range.x) / HEAT_MAX, 1.0)), 0.8);
    }
    // if debug is "difference between the renderers" show absolute difference
    if (debugView == 2) color = 16.0 * abs(color - clamp(texelFetch(otherImage, pixel, 0).rgb, 0.0, 1.0));
    fragColor = vec4(color, 1.0);       // draw the final color with full opacity
}
