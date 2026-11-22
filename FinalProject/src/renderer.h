#pragma once
#include <numeric>
#include <random>
#include <GL/glew.h>
#include <cyMatrix.h>
#include <cyGL.h>           // after cyMatrix.h, so that it can set matrix and vector uniforms
#include "splat_loader.h"
#include "scene.h"

const int       TILE_SIZE  = 16;            // these four match shaders/common.glsl
const GLuint    SORT_BLOCK = 4096;
const GLuint    SCAN_BLOCK = 1024;
const float     NO_MESH    = 1e30f;
const float     Z_NEAR     = 0.2f;          // near plane of the main camera: closer splats are culled
const GLuint    MAX_PAIRS  = 1u << 27;      // limit of the sort buffers: 2 GB
const int       RECORD_FLOATS = 16;         // size of struct Record in shaders/common.glsl
const cy::Vec3f BACKGROUND(0, 0, 0);

// Shader storage binding points, as numbered in shaders/common.glsl.
enum Binding { BIND_POSITIONS, BIND_COVARIANCES, BIND_SH, BIND_RECORDS, BIND_OFFSETS, BIND_FRAME_INFO, BIND_INDIRECT, BIND_KEYS,
               BIND_VALUES, BIND_KEYS_OUT, BIND_VALUES_OUT, BIND_HISTOGRAM, BIND_TILE_RANGES, BIND_SCAN_DATA, BIND_SCAN_SUMS,
               BIND_NORMALS, BIND_MESH_VERTICES };

// Position of each command in the indirect buffer, as written by setup.comp.
enum Indirect { INDIRECT_SORT = 0, INDIRECT_RANGES = 3, INDIRECT_DRAW = 6, INDIRECT_SIZE = 10 };

// Texture units, image units and uniform blocks, as in shaders/common.glsl.
enum TextureUnit { UNIT_IMAGE, UNIT_OTHER_IMAGE, UNIT_MESH_COLOR, UNIT_MESH_INFO, UNIT_SURFACE };
enum ImageUnit { IMAGE_COLOR, IMAGE_SURFACE };
enum UniformBlock { BLOCK_LIGHTS };

enum Pass { PASS_MESHES, PASS_PREPROCESS, PASS_SCAN, PASS_DUPLICATE, PASS_SORT, PASS_RANGES, PASS_RENDER, PASS_COUNT };
const char* PASS_NAMES[PASS_COUNT] = { "meshes", "preprocess", "scan", "duplicate", "sort", "ranges", "render" };

// Debug views, in the order of shaders/common.glsl.
enum DebugView { VIEW_IMAGE, VIEW_TILE_LOAD, VIEW_DIFFERENCE, VIEW_NORMALS, VIEW_FLATNESS, VIEW_LIGHT, VIEW_DEPTH, VIEW_COUNT };

// A view of the scene. focal is the focal length in pixels for a perspective camera, and the number of pixels per world
// unit for an orthographic one.
struct Camera {
    cy::Matrix4f view;          // world to view space (the camera looks down -z)
    cy::Matrix4f projection;    // for the meshes, the same projection as the splats
    cy::Vec3f    position;
    bool  orthographic = false;
    float focal;
    float zNear, zFar;          // depth range of the splats
};

// What the renderer draws and shows. main.cpp sets it from the keys.
struct RenderOptions {
    bool  useTiles = true;          // tile renderer (compute) or quad renderer (rasterization)
    bool  tightBounds = true;       // box where alpha >= 1/255, instead of the 3 sigma square of 3DGS
    bool  peakDepth = true;         // compare splats with meshes at their peak along the pixel ray, not at their center
    bool  computeColor = true;      // false when a view needs only the footprints and depths of the splats
    int   debugView = VIEW_IMAGE;
    int   selectedId = 0;           // id of the object to outline (index + 1), 0 for none
    float depthRange = 1;           // depth shown as red in the depth view
};

// The images and tile ranges of one view, sized for it: the screen, and in later steps the views from the lights.
struct RenderTarget {
    int    width = 0, height = 0, tilesX = 0, tilesY = 0;
    GLuint tileRangeBuffer = 0;                 // first pair and one past the last pair of each tile
    GLuint tileImage = 0, quadImage = 0;        // outputs of the two renderers
    GLuint surfaceImage = 0;                    // view depth of the visible surface, from the tile renderer
    GLuint meshColorImage = 0, meshInfoImage = 0, meshDepthImage = 0;   // meshes: color, (view depth, object id), depth buffer
    GLuint quadFramebuffer = 0, meshFramebuffer = 0;
};

// Pinhole camera at position looking at target, with the vertical field of view fovY for an image of width x height.
Camera perspectiveCamera(cy::Vec3f position, cy::Vec3f target, cy::Vec3f up, float fovY, int width, int height, float zFar) {
    Camera camera;
    camera.view = cy::Matrix4f::View(position, target, up);
    camera.position = position;
    camera.focal = 0.5f * height / std::tan(0.5f * fovY);
    camera.zNear = Z_NEAR;
    camera.zFar = zFar;
    camera.projection = cy::Matrix4f::Perspective(fovY, (float)width / height, Z_NEAR, zFar);
    return camera;
}

// Parallel camera looking along the unit vector direction, for a directional light. It sees the box around center that
// is halfSize high (half of the image height) and halfDepth deep in each direction.
Camera orthographicCamera(cy::Vec3f center, cy::Vec3f direction, cy::Vec3f up, float halfSize, float halfDepth, int width,
                          int height) {
    Camera camera;
    camera.position = center - direction * (1.1f * halfDepth);     // just outside the box, so that zNear stays above 0
    camera.view = cy::Matrix4f::View(camera.position, center, up);
    camera.orthographic = true;
    camera.focal = 0.5f * height / halfSize;
    camera.zNear = 0.1f * halfDepth;
    camera.zFar = 2.1f * halfDepth;
    float range = camera.zFar - camera.zNear;
    camera.projection = cy::Matrix4f(2 * camera.focal / width, 0, 0, 0,
                                     0, 2 * camera.focal / height, 0, 0,
                                     0, 0, -2 / range, -(camera.zFar + camera.zNear) / range,
                                     0, 0, 0, 1);
    return camera;
}

// The viewer as the shaders use it: (position, 1), or for an orthographic camera (direction toward the camera, 0).
cy::Vec4f eyeOf(const Camera& camera) {
    if (camera.orthographic) return cy::Vec4f(cy::Matrix3f(camera.view).GetRow(2), 0.0f);
    return cy::Vec4f(camera.position, 1.0f);
}

int    splatCount = 0;
GLuint pairCapacity = 0, maxSortBlocks = 0;             // pairs, and blocks of SORT_BLOCK pairs, that fit in the sort buffers
size_t gpuBytes = 0;                                    // size of all buffers
RenderTarget screen;                                    // the view shown in the window

GLuint positionBuffer, covarianceBuffer, shBuffer, normalBuffer;    // the model
GLuint meshVertexBuffer, lightBuffer;                   // vertices of the object shapes, light data
GLuint recordBuffer, offsetBuffer;                      // projected splats, pair count of each splat (then its prefix sum)
GLuint frameInfoBuffer, indirectBuffer;                 // pair count of the frame, indirect dispatch and draw commands
GLuint keyBuffer[2], valueBuffer[2], histogramBuffer;   // radix sort: pairs (ping-pong) and digit counts
GLuint scanLevelBuffer[3];                              // block sums of the prefix sum, one buffer per level
GLuint finalImage = 0, finalFramebuffer;                // the screen image after the debug view, copied to the window

cy::GLSLProgram preprocessProgram, scanReduceProgram, scanDownProgram, setupProgram, duplicateProgram, sortHistogramProgram,
                sortScatterProgram, tileRangesProgram, tileRenderProgram, quadProgram, presentProgram, meshProgram;

GLuint passQuery[PASS_COUNT];       // GPU timer of each pass
bool   passTimed[PASS_COUNT];       // the timer ran since its result was last read
double passMs[PASS_COUNT];          // smoothed GPU time of each pass
int    frameIndex = 0;
bool   timingEnabled = true;

// Number of groups of the given size that cover count items.
GLuint groups(GLuint count, GLuint size) {
    return (count + size - 1) / size;
}

// (Re)allocates a GPU buffer, optionally filled with data.
void allocateBuffer(GLuint buffer, size_t bytes, const void* data = nullptr) {
    GLint64 oldBytes = 0;
    glGetNamedBufferParameteri64v(buffer, GL_BUFFER_SIZE, &oldBytes);
    glNamedBufferData(buffer, bytes, data, GL_DYNAMIC_COPY);
    gpuBytes += bytes - oldBytes;
}

// Runs the bound compute program and makes its results visible to the passes that follow.
void dispatch(GLuint groupsX, GLuint groupsY = 1) {
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

// Same as dispatch, but the group count comes from a command in the indirect buffer.
void dispatchIndirect(Indirect command) {
    glDispatchComputeIndirect(command * sizeof(GLuint));
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

// Starts the GPU timer of a pass.
void beginPass(Pass pass) {
    if (!timingEnabled) return;
    glBeginQuery(GL_TIME_ELAPSED, passQuery[pass]);
    passTimed[pass] = true;
}

// Stops the GPU timer of the current pass.
void endPass() {
    if (timingEnabled) glEndQuery(GL_TIME_ELAPSED);
}

// Reads and smooths the GPU timers of the passes that ran in the previous frame. This waits until that frame is finished.
// A pass that did not run keeps its last time.
void updateTimers() {
    for (int pass = 0; pass < PASS_COUNT; pass++) {
        if (!passTimed[pass]) continue;
        GLuint64 nanoseconds = 0;
        glGetQueryObjectui64v(passQuery[pass], GL_QUERY_RESULT, &nanoseconds);
        passMs[pass] += 0.1 * (nanoseconds * 1e-6 - passMs[pass]);
        passTimed[pass] = false;
    }
}

// Compiles a compute shader file into a program.
bool buildCompute(cy::GLSLProgram& program, const char* file) {
    cy::GLSLShader shader;
    if (!shader.CompileFileEx(file, GL_COMPUTE_SHADER)) {
        std::cerr << "Cannot compile " << file << std::endl;
        return false;
    }
    program.CreateProgram();
    program.AttachShader(shader);
    return program.Link();
}

// Compiles all shader programs. Also used to reload them while the program runs.
bool buildPrograms() {
    struct { cy::GLSLProgram* program; const char* file; } compute[] = {
        { &preprocessProgram, "shaders/preprocess.comp" },       { &scanReduceProgram, "shaders/scan_reduce.comp" },
        { &scanDownProgram, "shaders/scan_down.comp" },          { &setupProgram, "shaders/setup.comp" },
        { &duplicateProgram, "shaders/duplicate.comp" },         { &sortHistogramProgram, "shaders/sort_histogram.comp" },
        { &sortScatterProgram, "shaders/sort_scatter.comp" },    { &tileRangesProgram, "shaders/tile_ranges.comp" },
        { &tileRenderProgram, "shaders/tile_render.comp" } };
    bool ok = true;
    for (auto& entry : compute) ok = buildCompute(*entry.program, entry.file) && ok;
    ok = quadProgram.BuildFilesEx("shaders/quad.vert", "shaders/quad.frag") && ok;
    ok = presentProgram.BuildFilesEx("shaders/present.vert", "shaders/present.frag") && ok;
    ok = meshProgram.BuildFilesEx("shaders/mesh.vert", "shaders/mesh.frag") && ok;
    return ok;
}

// Sizes the sort buffers for a number of (key, value) pairs.
void allocateSortBuffers(GLuint pairs) {
    maxSortBlocks = groups(pairs, SORT_BLOCK);
    pairCapacity = maxSortBlocks * SORT_BLOCK;
    for (int i = 0; i < 2; i++) {
        allocateBuffer(keyBuffer[i], (size_t)pairCapacity * sizeof(GLuint));
        allocateBuffer(valueBuffer[i], (size_t)pairCapacity * sizeof(GLuint));
    }
    allocateBuffer(histogramBuffer, (256 * (size_t)maxSortBlocks + 1) * sizeof(GLuint));

    // the scan levels must fit the two prefix sums: over the splats and over the histogram
    GLuint count = std::max((GLuint)splatCount, 256 * maxSortBlocks);
    for (GLuint buffer : scanLevelBuffer) {
        count = groups(count, SCAN_BLOCK);
        allocateBuffer(buffer, ((size_t)count + 1) * sizeof(GLuint));
    }
}

// Replaces a texture by an empty one of the given size.
void recreateTexture(GLuint& texture, GLenum format, int width, int height) {
    glDeleteTextures(1, &texture);
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, format, width, height);
}

// Sizes a render target: its tile grid, tile ranges and images. The first call also creates its buffer and framebuffers.
void resizeTarget(RenderTarget& target, int width, int height) {
    if (!target.meshFramebuffer) {
        glCreateBuffers(1, &target.tileRangeBuffer);
        glCreateFramebuffers(1, &target.quadFramebuffer);
        glCreateFramebuffers(1, &target.meshFramebuffer);
        const GLenum meshOutputs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glNamedFramebufferDrawBuffers(target.meshFramebuffer, 2, meshOutputs);
    }
    target.width = width;
    target.height = height;
    target.tilesX = groups(width, TILE_SIZE);
    target.tilesY = groups(height, TILE_SIZE);
    allocateBuffer(target.tileRangeBuffer, (size_t)target.tilesX * target.tilesY * 2 * sizeof(GLuint));
    recreateTexture(target.tileImage, GL_RGBA8, width, height);
    recreateTexture(target.quadImage, GL_RGBA16F, width, height);
    recreateTexture(target.surfaceImage, GL_R32F, width, height);
    recreateTexture(target.meshColorImage, GL_RGBA8, width, height);
    recreateTexture(target.meshInfoImage, GL_RG32F, width, height);
    recreateTexture(target.meshDepthImage, GL_DEPTH_COMPONENT32F, width, height);
    glNamedFramebufferTexture(target.quadFramebuffer, GL_COLOR_ATTACHMENT0, target.quadImage, 0);
    glNamedFramebufferTexture(target.meshFramebuffer, GL_COLOR_ATTACHMENT0, target.meshColorImage, 0);
    glNamedFramebufferTexture(target.meshFramebuffer, GL_COLOR_ATTACHMENT1, target.meshInfoImage, 0);
    glNamedFramebufferTexture(target.meshFramebuffer, GL_DEPTH_ATTACHMENT, target.meshDepthImage, 0);
}

// Sets the size of the screen image.
void resizeRenderer(int width, int height) {
    resizeTarget(screen, width, height);
    recreateTexture(finalImage, GL_RGBA8, width, height);
    glNamedFramebufferTexture(finalFramebuffer, GL_COLOR_ATTACHMENT0, finalImage, 0);
}

// Creates the GPU objects, uploads the model and compiles the shaders.
bool initRenderer(const SplatData& splats) {
    splatCount = splats.count;
    std::vector<float> meshVertices;
    if (!buildShapes(meshVertices)) return false;
    GLuint* buffers[] = { &positionBuffer, &covarianceBuffer, &shBuffer, &recordBuffer, &offsetBuffer, &frameInfoBuffer,
                          &indirectBuffer, &keyBuffer[0], &keyBuffer[1], &valueBuffer[0], &valueBuffer[1], &histogramBuffer,
                          &scanLevelBuffer[0], &scanLevelBuffer[1], &scanLevelBuffer[2], &normalBuffer, &meshVertexBuffer,
                          &lightBuffer };
    for (GLuint* buffer : buffers) glCreateBuffers(1, buffer);
    allocateBuffer(positionBuffer, splats.positions.size() * sizeof(float), splats.positions.data());
    allocateBuffer(covarianceBuffer, splats.covariances.size() * sizeof(float), splats.covariances.data());
    allocateBuffer(shBuffer, splats.sh.size() * sizeof(uint32_t), splats.sh.data());
    allocateBuffer(normalBuffer, splats.normals.size() * sizeof(float), splats.normals.data());
    allocateBuffer(meshVertexBuffer, meshVertices.size() * sizeof(float), meshVertices.data());
    allocateBuffer(lightBuffer, sizeof(LightBlock));
    allocateBuffer(recordBuffer, (size_t)splatCount * RECORD_FLOATS * sizeof(float));
    allocateBuffer(offsetBuffer, ((size_t)splatCount + 1) * sizeof(GLuint));
    allocateBuffer(frameInfoBuffer, 2 * sizeof(GLuint));
    allocateBuffer(indirectBuffer, INDIRECT_SIZE * sizeof(GLuint));
    allocateSortBuffers(1 << 24);

    // these buffers keep their binding point. The sort and scan buffers are bound by their passes
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_POSITIONS, positionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_COVARIANCES, covarianceBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_SH, shBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_RECORDS, recordBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_OFFSETS, offsetBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_FRAME_INFO, frameInfoBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INDIRECT, indirectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_HISTOGRAM, histogramBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_NORMALS, normalBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MESH_VERTICES, meshVertexBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, BLOCK_LIGHTS, lightBuffer);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);

    glCreateFramebuffers(1, &finalFramebuffer);
    glCreateQueries(GL_TIME_ELAPSED, PASS_COUNT, passQuery);
    GLuint vertexArray;     // has no attributes, since every draw reads its data from buffers by index
    glCreateVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);
    return buildPrograms();
}

// Exclusive prefix sum of count values, in place. The total is stored after the last value.
// The up sweep sums blocks of SCAN_BLOCK values level by level, then the down sweep turns each level into running sums.
void gpuScan(GLuint buffer, GLuint count) {
    GLuint levelBuffer[4] = { buffer, scanLevelBuffer[0], scanLevelBuffer[1], scanLevelBuffer[2] };
    GLuint levelCount[4] = { count };
    int top = 0;
    for (; levelCount[top] > SCAN_BLOCK; top++) {
        levelCount[top + 1] = groups(levelCount[top], SCAN_BLOCK);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_SCAN_DATA, levelBuffer[top]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_SCAN_SUMS, levelBuffer[top + 1]);
        scanReduceProgram["count"] = levelCount[top];
        dispatch(groups(levelCount[top + 1], 64));
    }
    for (int level = top; level >= 0; level--) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_SCAN_DATA, levelBuffer[level]);
        if (level < top) glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_SCAN_SUMS, levelBuffer[level + 1]);
        scanDownProgram["count"] = levelCount[level];
        scanDownProgram["hasParent"] = int(level < top);
        dispatch(groups(groups(levelCount[level], SCAN_BLOCK), 64));
    }
}

// Stable radix sort of the pairs by key: four passes of 8 bits, each one counting, scanning and moving the pairs.
// The pairs go back and forth between the two buffers and end in keyBuffer[0] and valueBuffer[0].
void gpuSort() {
    for (int pass = 0; pass < 4; pass++) {
        int in = pass & 1, out = in ^ 1;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_KEYS, keyBuffer[in]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_VALUES, valueBuffer[in]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_KEYS_OUT, keyBuffer[out]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_VALUES_OUT, valueBuffer[out]);

        // Clear the histogram buffer, set the digit shift and the number of blocks, and run the histogram pass.
        glClearNamedBufferData(histogramBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        sortHistogramProgram["shift"] = 8 * pass;
        sortHistogramProgram["maxBlocks"] = maxSortBlocks;
        dispatchIndirect(INDIRECT_SORT);

        gpuScan(histogramBuffer, 256 * maxSortBlocks);  //Histogram prefix sum: the start of each digit in each block

        // Set the digit shift and the number of blocks, and run the scatter pass.
        sortScatterProgram["shift"] = 8 * pass;
        sortScatterProgram["maxBlocks"] = maxSortBlocks;
        dispatchIndirect(INDIRECT_SORT);
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_KEYS, keyBuffer[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_VALUES, valueBuffer[0]);
}

// Copies the lights of the scene to the uniform block of the shaders. Call it at the start of every frame.
void uploadLights() {
    LightBlock lights = packLights();
    glNamedBufferSubData(lightBuffer, 0, sizeof(lights), &lights);
}

// Draws the shapes of the objects with the given program. Shadow passes leave out the shapes of the lights,
// which would otherwise block their own light.
void drawObjects(cy::GLSLProgram& program, bool lightShapes) {
    for (int i = 0; i < (int)objects.size(); i++) {
        const SceneObject& object = objects[i];
        bool isLight = object.type != OBJECT_MESH;
        if (isLight && !lightShapes) continue;
        program["model"] = cy::Matrix4f(object.rotation * object.size, object.position);
        program["color"] = PALETTE[object.color] * (isLight ? 1.0f : 0.8f);
        program["unlit"] = (int)isLight;
        program["objectId"] = i + 1;
        glDrawArrays(GL_TRIANGLES, shapes[object.type].first, shapes[object.type].count);
    }
}

// Draws the objects into the mesh images of a target (color over the background, view depth, object id).
void renderMeshes(const Camera& camera, const RenderTarget& target, cy::Vec3f up, const RenderOptions& options) {
    beginPass(PASS_MESHES);
    const float background[4] = { BACKGROUND.x, BACKGROUND.y, BACKGROUND.z, 1 }, noMesh[4] = { NO_MESH, 0, 0, 0 }, farthest = 1;
    glBindFramebuffer(GL_FRAMEBUFFER, target.meshFramebuffer);
    glViewport(0, 0, target.width, target.height);
    glClearBufferfv(GL_COLOR, 0, background);
    glClearBufferfv(GL_COLOR, 1, noMesh);
    glClearBufferfv(GL_DEPTH, 0, &farthest);
    glEnable(GL_DEPTH_TEST);
    meshProgram["view"] = camera.view;
    meshProgram["projection"] = camera.projection;
    meshProgram["eye"] = eyeOf(camera);
    meshProgram["up"] = up;
    meshProgram["debugView"] = options.debugView;
    drawObjects(meshProgram, showLightShapes);
    glDisable(GL_DEPTH_TEST);
    endPass();
}

// Renders the splats of a view over its meshes, into the tile image (compute tile renderer) or the quad image (instanced quads).
void renderSplats(const Camera& camera, const RenderTarget& target, const RenderOptions& options) {
    int tileBits = 1;
    while ((1 << tileBits) < target.tilesX * target.tilesY) tileBits++;
    int depthBits = std::min(32 - tileBits, 24);    // a float code keeps 24 bits
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_TILE_RANGES, target.tileRangeBuffer);

    // run preprocess pass to compute the splat pairs and their tight bounds
    beginPass(PASS_PREPROCESS);
    preprocessProgram["splatCount"] = (GLuint)splatCount;
    preprocessProgram["view"] = camera.view;
    preprocessProgram["eye"] = eyeOf(camera);
    preprocessProgram["orthographic"] = (int)camera.orthographic;
    preprocessProgram["viewport"].Set(target.width, target.height);
    preprocessProgram["focal"] = camera.focal;
    preprocessProgram["zNear"] = camera.zNear;
    preprocessProgram["tightBounds"] = (int)options.tightBounds;
    preprocessProgram["tileMode"] = (int)options.useTiles;
    preprocessProgram["computeColor"] = (int)options.computeColor;
    preprocessProgram["relight"] = int(lightCount() > 0 || ambient != 1);
    preprocessProgram["debugView"] = options.debugView;
    dispatch(groups(splatCount, 256));
    endPass();

    // run scan pass to compute the prefix sum of the splat pairs
    beginPass(PASS_SCAN);
    gpuScan(offsetBuffer, splatCount);
    setupProgram["splatCount"] = (GLuint)splatCount;
    setupProgram["capacity"] = pairCapacity;
    dispatch(1);
    endPass();

    // run duplicate pass to copy the splat pairs into the sort buffers
    beginPass(PASS_DUPLICATE);
    duplicateProgram["splatCount"] = (GLuint)splatCount;
    duplicateProgram["viewport"].Set(target.width, target.height);
    duplicateProgram["tilesX"] = target.tilesX;
    duplicateProgram["depthBits"] = depthBits;
    duplicateProgram["zNear"] = camera.zNear;
    duplicateProgram["zFar"] = camera.zFar;
    duplicateProgram["tileMode"] = (int)options.useTiles;
    duplicateProgram["capacity"] = pairCapacity;
    dispatch(groups(splatCount, 256));
    endPass();

    // run sort pass to sort the splat pairs by depth
    beginPass(PASS_SORT);
    gpuSort();
    endPass();

    // run ranges pass to compute the range of pairs that cover each tile
    beginPass(PASS_RANGES);
    if (options.useTiles) {
        glClearNamedBufferData(target.tileRangeBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        tileRangesProgram["depthBits"] = depthBits;
        dispatchIndirect(INDIRECT_RANGES);
    }
    endPass();

    // run render pass to draw the splats into the output image
    beginPass(PASS_RENDER);
    glBindTextureUnit(UNIT_MESH_COLOR, target.meshColorImage);
    glBindTextureUnit(UNIT_MESH_INFO, target.meshInfoImage);
    if (options.useTiles) {
        glBindImageTexture(IMAGE_COLOR, target.tileImage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glBindImageTexture(IMAGE_SURFACE, target.surfaceImage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        tileRenderProgram["viewport"].Set(target.width, target.height);
        tileRenderProgram["tilesX"] = target.tilesX;
        tileRenderProgram["peakDepth"] = (int)options.peakDepth;
        dispatch(target.tilesX, target.tilesY);
    } else {
        // the quads blend over a copy of the mesh image
        glBlitNamedFramebuffer(target.meshFramebuffer, target.quadFramebuffer, 0, 0, target.width, target.height, 0, 0,
                               target.width, target.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, target.quadFramebuffer);
        glViewport(0, 0, target.width, target.height);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        quadProgram["viewport"].Set(target.width, target.height);
        quadProgram["peakDepth"] = (int)options.peakDepth;
        glDrawArraysIndirect(GL_TRIANGLE_STRIP, (void*)(INDIRECT_DRAW * sizeof(GLuint)));
        glDisable(GL_BLEND);
    }
    endPass();
}

// Applies the debug view and the selection outline to the screen image, then copies it to the window, scaled to fit.
void present(const RenderOptions& options, int windowWidth, int windowHeight) {
    glBindFramebuffer(GL_FRAMEBUFFER, finalFramebuffer);
    glViewport(0, 0, screen.width, screen.height);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_TILE_RANGES, screen.tileRangeBuffer);
    glBindTextureUnit(UNIT_IMAGE, options.useTiles ? screen.tileImage : screen.quadImage);
    glBindTextureUnit(UNIT_OTHER_IMAGE, options.useTiles ? screen.quadImage : screen.tileImage);
    glBindTextureUnit(UNIT_MESH_INFO, screen.meshInfoImage);
    glBindTextureUnit(UNIT_SURFACE, screen.surfaceImage);
    presentProgram["debugView"] = options.debugView;
    presentProgram["tilesX"] = screen.tilesX;
    presentProgram["selectedId"] = options.selectedId;
    presentProgram["depthRange"] = options.depthRange;
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    float scale = std::min((float)windowWidth / screen.width, (float)windowHeight / screen.height);
    int width = int(screen.width * scale), height = int(screen.height * scale);
    int x = (windowWidth - width) / 2, y = (windowHeight - height) / 2;
    glBlitNamedFramebuffer(finalFramebuffer, 0, 0, 0, screen.width, screen.height, x, y, x + width, y + height,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

// Largest and mean difference between the outputs of the two renderers in a target, in 8-bit color levels.
void compareRenderers(const RenderTarget& target, int& largest, double& mean) {
    size_t bytes = (size_t)target.width * target.height * 4;
    std::vector<unsigned char> tile(bytes), quad(bytes);
    glGetTextureImage(target.tileImage, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)bytes, tile.data());
    glGetTextureImage(target.quadImage, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)bytes, quad.data());
    largest = 0;
    mean = 0;
    for (size_t i = 0; i < bytes; i++) {
        int difference = std::abs(tile[i] - quad[i]);
        largest = std::max(largest, difference);
        mean += (double)difference / bytes;
    }
}

// Reads one pixel of an image of the last frame.
cy::Vec4f readPixel(GLuint image, int x, int y) {
    cy::Vec4f value(0, 0, 0, 0);
    glGetTextureSubImage(image, 0, x, y, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(value), &value);
    return value;
}

// Reads back count 32-bit values of a GPU buffer.
std::vector<GLuint> readBuffer(GLuint buffer, size_t count) {
    std::vector<GLuint> data(count);
    glGetNamedBufferSubData(buffer, 0, count * sizeof(GLuint), data.data());
    return data;
}

// Checks the GPU prefix sum and the GPU sort against the CPU. It overwrites the buffers of the frame, so call it between frames.
void selfTest() {
    std::vector<GLuint> info = readBuffer(frameInfoBuffer, 2);
    std::vector<GLuint> keys = readBuffer(keyBuffer[0], info[0]);
    std::cout << "keys of the last frame in order: " << (std::is_sorted(keys.begin(), keys.end()) ? "yes" : "NO")
              << " (" << info[0] << " pairs)" << std::endl;

    // prefix sum of random counts, of the same size as the one over the splats
    std::mt19937 random(1);
    std::vector<GLuint> counts(splatCount + 1), sums(splatCount + 1);
    for (int i = 0; i < splatCount; i++) {
        counts[i] = random() % 16;
        sums[i + 1] = sums[i] + counts[i];
    }
    glNamedBufferSubData(offsetBuffer, 0, counts.size() * sizeof(GLuint), counts.data());
    gpuScan(offsetBuffer, splatCount);
    std::cout << "prefix sum: " << (readBuffer(offsetBuffer, sums.size()) == sums ? "correct" : "WRONG") << std::endl;

    // sort of random keys with many duplicates. A stable sort on the CPU must give the same order
    GLuint count = std::min(pairCapacity, 3000000u) - 123;
    std::vector<GLuint> values(count), expected(count);
    keys.resize(count);
    for (GLuint i = 0; i < count; i++) {
        keys[i] = (random() % 65536) * 65537;
        values[i] = i;
    }
    std::iota(expected.begin(), expected.end(), 0);
    std::stable_sort(expected.begin(), expected.end(), [&](GLuint a, GLuint b) { return keys[a] < keys[b]; });
    GLuint frameInfo[2] = { count, count }, sortCommand[3] = { groups(count, SORT_BLOCK), 1, 1 };
    glNamedBufferSubData(frameInfoBuffer, 0, sizeof(frameInfo), frameInfo);
    glNamedBufferSubData(indirectBuffer, INDIRECT_SORT * sizeof(GLuint), sizeof(sortCommand), sortCommand);
    glNamedBufferSubData(keyBuffer[0], 0, count * sizeof(GLuint), keys.data());
    glNamedBufferSubData(valueBuffer[0], 0, count * sizeof(GLuint), values.data());
    gpuSort();
    std::vector<GLuint> sortedKeys = readBuffer(keyBuffer[0], count);
    bool correct = readBuffer(valueBuffer[0], count) == expected;
    for (GLuint i = 0; i < count; i++) correct = correct && sortedKeys[i] == keys[expected[i]];
    std::cout << "sort: " << (correct ? "correct" : "WRONG") << std::endl;
}
