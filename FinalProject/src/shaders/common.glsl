const int   TILE_SIZE  = 16;            // tile side in pixels
const uint  SORT_BLOCK = 4096u;         // pairs sorted by one workgroup
const uint  SCAN_BLOCK = 1024u;         // values summed by one thread
const float ALPHA_MIN  = 1.0 / 255.0;   // smallest alpha that is blended
const float ISO_T      = 0.5;           // the visible surface of the splats is where the transmittance falls below this
const float NO_MESH    = 1e30;          // mesh depth of the pixels without a mesh
const float PEAK_RANGE = 0.05;          // the peak depth of a splat stays within this fraction of its center depth

// Debug views, in the order of renderer.h.
const int VIEW_IMAGE = 0, VIEW_TILE_LOAD = 1, VIEW_DIFFERENCE = 2, VIEW_NORMALS = 3, VIEW_FLATNESS = 4, VIEW_LIGHT = 5,
          VIEW_DEPTH = 6;

// Texture units, image units and uniform blocks, as in renderer.h.
const int UNIT_IMAGE = 0, UNIT_OTHER_IMAGE = 1, UNIT_MESH_COLOR = 2, UNIT_MESH_INFO = 3, UNIT_SURFACE = 4;
const int IMAGE_COLOR = 0, IMAGE_SURFACE = 1;
const int BLOCK_LIGHTS = 0;

// A splat projected to the screen, written by preprocess.comp. Update RECORD_FLOATS in renderer.h when it changes.
struct Record {
    vec4 meanExtent;      // xy: mean in pixels, zw: half size of the bounding box
    vec4 conicOpacity;    // xyz: inverse 2D covariance (a, b, c), w: opacity
    vec4 colorDepth;      // rgb: color seen from the camera, a: view depth
    vec4 depthSlope;      // xy: change of the peak depth per pixel away from the mean
};

//     std430 gives float and uint arrays a 4-byte stride so they match packed C++ arrays
// The driver allows 16 storage blocks per shader, so a shader defines the groups it needs (USE_SPLATS, ...) before
// including this file. The binding numbers match enum Binding in renderer.h.
#ifdef USE_SPLATS
layout(std430, binding = 0)  buffer SplatPositions   { vec4   splatPosition[]; };     // xyz: mean, w: opacity
layout(std430, binding = 1)  buffer SplatCovariances { float  splatCovariance[]; };   // 6 per splat: xx xy xz yy yz zz
layout(std430, binding = 2)  buffer SplatSH          { uint   splatSH[]; };           // 24 per splat: 48 half floats
layout(std430, binding = 15) buffer SplatNormals     { vec4   splatNormal[]; };       // xyz: shortest axis, w: flatness
#endif
#ifdef USE_RECORDS
layout(std430, binding = 3)  buffer Records          { Record record[]; };
#endif
#ifdef USE_COUNTS
layout(std430, binding = 4)  buffer SplatOffsets     { uint   splatOffset[]; };       // pair count of each splat, then its prefix sum with the total at the end
layout(std430, binding = 5)  buffer FrameInfo        { uint pairCount; uint pairTotal; };   // pairs that are sorted, pairs that were requested
layout(std430, binding = 6)  buffer IndirectCommands { uint   indirect[]; };          // dispatch sizes and draw command, see setup.comp
#endif
#ifdef USE_PAIRS
layout(std430, binding = 7)  buffer SortKeys         { uint   sortKey[]; };           // tile and depth of each pair
layout(std430, binding = 8)  buffer SortValues       { uint   sortValue[]; };         // splat index of each pair
#endif
#ifdef USE_SORT
layout(std430, binding = 9)  buffer SortKeysOut      { uint   sortKeyOut[]; };
layout(std430, binding = 10) buffer SortValuesOut    { uint   sortValueOut[]; };
layout(std430, binding = 11) buffer Histogram        { uint   histogram[]; };         // [digit * maxBlocks + block]
#endif
#ifdef USE_TILES
layout(std430, binding = 12) buffer TileRanges       { uvec2  tileRange[]; };         // first pair and one past the last pair of each tile
#endif
#ifdef USE_SCAN
layout(std430, binding = 13) buffer ScanData         { uint   scanData[]; };
layout(std430, binding = 14) buffer ScanSums         { uint   scanSum[]; };
#endif
#ifdef USE_MESH
struct MeshVertex {
    vec4 position;      // in the space of its shape
    vec4 normal;
};
layout(std430, binding = 16) buffer MeshVertices     { MeshVertex meshVertex[]; };    // all shapes, one after the other
#endif

// Tiles touched by the bounding box of a splat: xy = first tile, zw = last tile (empty if z < x).
// Pixel centers have integer coordinates, so pixel i is inside the box when mean - extent <= i <= mean + extent.
ivec4 tileRect(vec2 mean, vec2 extent, ivec2 viewport) {
    ivec2 first = ivec2(clamp(ceil(mean - extent), vec2(0.0), vec2(viewport)));
    ivec2 last  = ivec2(clamp(floor(mean + extent), vec2(-1.0), vec2(viewport - 1)));
    if (any(lessThan(last, first))) return ivec4(0, 0, -1, -1);
    return ivec4(first / TILE_SIZE, last / TILE_SIZE);
}
