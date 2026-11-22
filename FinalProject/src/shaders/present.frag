#version 460 core
#define USE_TILES
#include "common.glsl"

layout(binding = UNIT_IMAGE) uniform sampler2D image;               // output of the active renderer
layout(binding = UNIT_OTHER_IMAGE) uniform sampler2D otherImage;     // output of the other renderer
layout(binding = UNIT_SURFACE) uniform sampler2D surfaceImage;       // view depth of the visible surface, from the tile renderer
layout(binding = UNIT_MESH_INFO) uniform sampler2D meshInfo;         // g: id of the object drawn at each pixel
uniform int debugView;          // one of the VIEW_ constants of common.glsl
uniform int tilesX;
uniform int selectedId;         // id of the selected object, 0 for none
uniform float depthRange;       // depth shown as red in the depth view

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
    if (debugView == VIEW_TILE_LOAD) {           // if debug is "splats per tile" show heat map
        ivec2 tile = pixel / TILE_SIZE;
        uvec2 range = tileRange[tile.y * tilesX + tile.x];
        color = mix(color, heat(min(float(range.y - range.x) / HEAT_MAX, 1.0)), 0.8);
    }
    // if debug is "difference between the renderers" show absolute difference
    if (debugView == VIEW_DIFFERENCE) color = 16.0 * abs(color - clamp(texelFetch(otherImage, pixel, 0).rgb, 0.0, 1.0));
    if (debugView == VIEW_DEPTH) {
        float depth = texelFetch(surfaceImage, pixel, 0).r;
        color = depth > 0.0 ? heat(min(depth / depthRange, 1.0)) : vec3(0.0);
    }

    // outline of the selected object: pixels where some, but not all, of the four neighbors two pixels away belong to it
    if (selectedId > 0) {
        const ivec2 NEIGHBOR[4] = ivec2[](ivec2(2, 0), ivec2(-2, 0), ivec2(0, 2), ivec2(0, -2));
        int inside = 0;
        for (int k = 0; k < 4; k++) {
            ivec2 p = clamp(pixel + NEIGHBOR[k], ivec2(0), textureSize(meshInfo, 0) - 1);
            inside += int(texelFetch(meshInfo, p, 0).g) == selectedId ? 1 : 0;
        }
        if (inside > 0 && inside < 4) color = vec3(1.0, 0.85, 0.1);
    }
    fragColor = vec4(color, 1.0);       // draw the final color with full opacity
}
