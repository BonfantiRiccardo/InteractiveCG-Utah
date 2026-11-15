const int   TILE_SIZE  = 16;            // tile side in pixels
const uint  SORT_BLOCK = 4096u;         // pairs sorted by one workgroup
const uint  SCAN_BLOCK = 1024u;         // values summed by one thread
const float Z_NEAR     = 0.2;           // splats closer than this are culled
const float ALPHA_MIN  = 1.0 / 255.0;   // smallest alpha that is blended

// A splat projected to the screen, written by preprocess.comp.
struct Record {
    vec4 meanExtent;      // xy: mean in pixels, zw: half size of the bounding box
    vec4 conicOpacity;    // xyz: inverse 2D covariance (a, b, c), w: opacity
    vec4 colorDepth;      // rgb: color seen from the camera, a: view depth
};

//     std430 gives float and uint arrays a 4-byte stride so they match packed C++ arrays
layout(std430, binding = 0)  buffer SplatPositions   { vec4   splatPosition[]; };     // xyz: mean, w: opacity
layout(std430, binding = 1)  buffer SplatCovariances { float  splatCovariance[]; };   // 6 per splat: xx xy xz yy yz zz
layout(std430, binding = 2)  buffer SplatSH          { uint   splatSH[]; };           // 24 per splat: 48 half floats
layout(std430, binding = 3)  buffer Records          { Record record[]; };
layout(std430, binding = 4)  buffer SplatOffsets     { uint   splatOffset[]; };       // pairs of each splat, then their prefix sum; last entry: total
layout(std430, binding = 5)  buffer FrameInfo        { uint pairCount; uint pairTotal; };   // pairs that are sorted, pairs that were requested
layout(std430, binding = 6)  buffer IndirectCommands { uint   indirect[]; };          // dispatch sizes and draw command, see setup.comp
layout(std430, binding = 7)  buffer SortKeys         { uint   sortKey[]; };           // tile and depth of each pair
layout(std430, binding = 8)  buffer SortValues       { uint   sortValue[]; };         // splat index of each pair
layout(std430, binding = 9)  buffer SortKeysOut      { uint   sortKeyOut[]; };
layout(std430, binding = 10) buffer SortValuesOut    { uint   sortValueOut[]; };
layout(std430, binding = 11) buffer Histogram        { uint   histogram[]; };         // [digit * maxBlocks + block]
layout(std430, binding = 12) buffer TileRanges       { uvec2  tileRange[]; };         // first pair and one past the last pair of each tile
layout(std430, binding = 13) buffer ScanData         { uint   scanData[]; };
layout(std430, binding = 14) buffer ScanSums         { uint   scanSum[]; };

// Tiles touched by the bounding box of a splat: xy = first tile, zw = last tile (empty if z < x).
// Pixel centers have integer coordinates, so pixel i is inside the box when mean - extent <= i <= mean + extent.
ivec4 tileRect(vec2 mean, vec2 extent, ivec2 viewport) {
    ivec2 first = ivec2(clamp(ceil(mean - extent), vec2(0.0), vec2(viewport)));
    ivec2 last  = ivec2(clamp(floor(mean + extent), vec2(-1.0), vec2(viewport - 1)));
    if (any(lessThan(last, first))) return ivec4(0, 0, -1, -1);
    return ivec4(first / TILE_SIZE, last / TILE_SIZE);
}
