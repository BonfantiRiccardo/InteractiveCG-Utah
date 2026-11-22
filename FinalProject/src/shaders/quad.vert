#version 460 core
#define USE_RECORDS
#define USE_COUNTS
#define USE_PAIRS
#include "common.glsl"

uniform ivec2 viewport;

out vec2 offset;                // from the splat mean, in pixels
flat out vec4 conicOpacity;
flat out vec4 colorDepth;
flat out vec2 slope;

// One quad per sorted pair, covering the bounding box of its splat. Instances are drawn back to front.
void main() {
    Record r = record[sortValue[pairCount - 1u - uint(gl_InstanceID)]];
    vec2 corner = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0 - 1.0;
    offset = corner * r.meanExtent.zw;
    conicOpacity = r.conicOpacity;
    colorDepth = r.colorDepth;
    slope = r.depthSlope.xy;
    gl_Position = vec4((r.meanExtent.xy + offset + 0.5) / vec2(viewport) * 2.0 - 1.0, 0.0, 1.0);
}
