#pragma once
#include <numeric>
#include <random>
#include <GL/glew.h>
#include <cyMatrix.h>
#include <cyGL.h>           // after cyMatrix.h, so that it can set matrix and vector uniforms
#include "splat_loader.h"

const int       TILE_SIZE  = 16;            // these three match shaders/common.glsl
const GLuint    SORT_BLOCK = 4096;
const GLuint    SCAN_BLOCK = 1024;
const GLuint    MAX_PAIRS  = 1u << 27;      // limit of the sort buffers: 2 GB
const cy::Vec3f BACKGROUND(0, 0, 0);

// Shader storage binding points, in the order of shaders/common.glsl.
enum Binding { BIND_POSITIONS, BIND_COVARIANCES, BIND_SH, BIND_RECORDS, BIND_OFFSETS, BIND_FRAME_INFO, BIND_INDIRECT, BIND_KEYS,
               BIND_VALUES, BIND_KEYS_OUT, BIND_VALUES_OUT, BIND_HISTOGRAM, BIND_TILE_RANGES, BIND_SCAN_DATA, BIND_SCAN_SUMS };

// Position of each command in the indirect buffer, as written by setup.comp.
enum Indirect { INDIRECT_SORT = 0, INDIRECT_RANGES = 3, INDIRECT_DRAW = 6, INDIRECT_SIZE = 10 };

enum Pass { PASS_PREPROCESS, PASS_SCAN, PASS_DUPLICATE, PASS_SORT, PASS_RANGES, PASS_RENDER, PASS_COUNT };
const char* PASS_NAMES[PASS_COUNT] = { "preprocess", "scan", "duplicate", "sort", "ranges", "render" };

struct Camera {
    cy::Matrix4f view;      // world to view space (the camera looks down -z)
    cy::Vec3f position;
    float focal;            // focal length in pixels
    float zFar;             // at least the depth of the farthest splat
};

int    splatCount = 0;
int    renderWidth = 0, renderHeight = 0, tilesX = 0, tilesY = 0;
GLuint pairCapacity = 0, maxSortBlocks = 0;             // pairs, and blocks of SORT_BLOCK pairs, that fit in the sort buffers
size_t gpuBytes = 0;                                    // size of all buffers

GLuint positionBuffer, covarianceBuffer, shBuffer;      // the model
GLuint recordBuffer, offsetBuffer;                      // projected splats; pairs of each splat, then their prefix sum
GLuint frameInfoBuffer, indirectBuffer;                 // pair count of the frame; indirect dispatch and draw commands
GLuint keyBuffer[2], valueBuffer[2], histogramBuffer;   // radix sort: pairs (ping-pong) and digit counts
GLuint scanLevelBuffer[3];                              // block sums of the prefix sum, one buffer per level
GLuint tileRangeBuffer;
GLuint tileImage = 0, quadImage = 0, finalImage = 0;    // outputs of the two renderers; image copied to the window
GLuint quadFramebuffer, finalFramebuffer;

cy::GLSLProgram preprocessProgram, scanReduceProgram, scanDownProgram, setupProgram, duplicateProgram, sortHistogramProgram,
                sortScatterProgram, tileRangesProgram, tileRenderProgram, quadProgram, presentProgram;

GLuint passQuery[PASS_COUNT];       // GPU timer of each pass
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
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

// Runs the bound compute program with an indirect command and makes its results visible to the passes that follow.
void dispatchIndirect(Indirect command) {
    glDispatchComputeIndirect(command * sizeof(GLuint));
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

// Starts the GPU timer of a pass.
void beginPass(Pass pass) {
    if (timingEnabled) glBeginQuery(GL_TIME_ELAPSED, passQuery[pass]);
}

// Stops the GPU timer of the current pass.
void endPass() {
    if (timingEnabled) glEndQuery(GL_TIME_ELAPSED);
}

// Reads the GPU timers of the previous frame, which waits until that frame is finished, and smooths them.
void updateTimers() {
    if (frameIndex == 0) return;        //skip first frame
    for (int pass = 0; pass < PASS_COUNT; pass++) {
        GLuint64 nanoseconds = 0;
        glGetQueryObjectui64v(passQuery[pass], GL_QUERY_RESULT, &nanoseconds);
        passMs[pass] += 0.1 * (nanoseconds * 1e-6 - passMs[pass]);
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

// Replaces a texture by an empty one of the render size.
void recreateTexture(GLuint& texture, GLenum format) {
    glDeleteTextures(1, &texture);
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, format, renderWidth, renderHeight);
}

// Sets the render size: render targets and tile grid.
void resizeRenderer(int width, int height) {
    renderWidth = width;
    renderHeight = height;
    tilesX = groups(width, TILE_SIZE);
    tilesY = groups(height, TILE_SIZE);
    allocateBuffer(tileRangeBuffer, (size_t)tilesX * tilesY * 2 * sizeof(GLuint));
    recreateTexture(tileImage, GL_RGBA8);
    recreateTexture(quadImage, GL_RGBA16F);
    recreateTexture(finalImage, GL_RGBA8);
    glNamedFramebufferTexture(quadFramebuffer, GL_COLOR_ATTACHMENT0, quadImage, 0);
    glNamedFramebufferTexture(finalFramebuffer, GL_COLOR_ATTACHMENT0, finalImage, 0);
}

// Creates the GPU objects, uploads the model and compiles the shaders.
bool initRenderer(const SplatData& splats) {
    splatCount = splats.count;
    GLuint* buffers[] = { &positionBuffer, &covarianceBuffer, &shBuffer, &recordBuffer, &offsetBuffer, &frameInfoBuffer,
                          &indirectBuffer, &keyBuffer[0], &keyBuffer[1], &valueBuffer[0], &valueBuffer[1], &histogramBuffer,
                          &tileRangeBuffer, &scanLevelBuffer[0], &scanLevelBuffer[1], &scanLevelBuffer[2] };
    for (GLuint* buffer : buffers) glCreateBuffers(1, buffer);
    allocateBuffer(positionBuffer, splats.positions.size() * sizeof(float), splats.positions.data());
    allocateBuffer(covarianceBuffer, splats.covariances.size() * sizeof(float), splats.covariances.data());
    allocateBuffer(shBuffer, splats.sh.size() * sizeof(uint32_t), splats.sh.data());
    allocateBuffer(recordBuffer, (size_t)splatCount * 12 * sizeof(float));
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
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_TILE_RANGES, tileRangeBuffer);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);

    glCreateFramebuffers(1, &quadFramebuffer);
    glCreateFramebuffers(1, &finalFramebuffer);
    glCreateQueries(GL_TIME_ELAPSED, PASS_COUNT, passQuery);
    GLuint vertexArray;     // the quads and the fullscreen triangle have no vertex attributes
    glCreateVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);
    return buildPrograms();
}

// Exclusive prefix sum of count values, in place; the total is stored after the last value.
// Level l + 1 holds one sum per SCAN_BLOCK values of level l. The up sweep reduces until one block is left,
// then the down sweep turns each level into running sums, from the top.
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

// Sorts the pairs by key: a stable radix sort with four passes of 8 bits, from the lowest digit.
// Each pass counts the digits per block, turns the counts into start positions with a prefix sum, and moves the pairs.
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

// Renders the splats into tileImage (compute tile renderer) or into quadImage (instanced quads).
void renderSplats(const Camera& camera, bool useTiles, bool tightBounds) {
    int tileBits = 1;
    while ((1 << tileBits) < tilesX * tilesY) tileBits++;
    int depthBits = std::min(32 - tileBits, 24);    // a float code keeps 24 bits

    // run preprocess pass to compute the splat pairs and their tight bounds
    beginPass(PASS_PREPROCESS);
    preprocessProgram["splatCount"] = (GLuint)splatCount;
    preprocessProgram["view"] = camera.view;
    preprocessProgram["cameraPosition"] = camera.position;
    preprocessProgram["viewport"].Set(renderWidth, renderHeight);
    preprocessProgram["focal"] = camera.focal;
    preprocessProgram["tightBounds"] = (int)tightBounds;
    preprocessProgram["tileMode"] = (int)useTiles;
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
    duplicateProgram["viewport"].Set(renderWidth, renderHeight);
    duplicateProgram["tilesX"] = tilesX;
    duplicateProgram["depthBits"] = depthBits;
    duplicateProgram["zFar"] = camera.zFar;
    duplicateProgram["tileMode"] = (int)useTiles;
    duplicateProgram["capacity"] = pairCapacity;
    dispatch(groups(splatCount, 256));
    endPass();

    // run sort pass to sort the splat pairs by depth
    beginPass(PASS_SORT);
    gpuSort();
    endPass();

    // run ranges pass to compute the range of pairs that cover each tile
    beginPass(PASS_RANGES);
    if (useTiles) {
        glClearNamedBufferData(tileRangeBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        tileRangesProgram["depthBits"] = depthBits;
        dispatchIndirect(INDIRECT_RANGES);
    }
    endPass();

    // run render pass to draw the splats into the output image
    beginPass(PASS_RENDER);
    if (useTiles) {
        glBindImageTexture(0, tileImage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        tileRenderProgram["viewport"].Set(renderWidth, renderHeight);
        tileRenderProgram["tilesX"] = tilesX;
        tileRenderProgram["background"] = BACKGROUND;
        dispatch(tilesX, tilesY);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, quadFramebuffer);
        glViewport(0, 0, renderWidth, renderHeight);
        glClearColor(BACKGROUND.x, BACKGROUND.y, BACKGROUND.z, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        quadProgram["viewport"].Set(renderWidth, renderHeight);
        glDrawArraysIndirect(GL_TRIANGLE_STRIP, (void*)(INDIRECT_DRAW * sizeof(GLuint)));
        glDisable(GL_BLEND);
    }
    endPass();
}

// Applies the debug view to the output of a renderer and copies the result to the window, scaled to fit.
void present(bool useTiles, int debugView, int windowWidth, int windowHeight) {
    glBindFramebuffer(GL_FRAMEBUFFER, finalFramebuffer);
    glViewport(0, 0, renderWidth, renderHeight);
    glBindTextureUnit(0, useTiles ? tileImage : quadImage);
    glBindTextureUnit(1, useTiles ? quadImage : tileImage);
    presentProgram["image"] = 0;
    presentProgram["otherImage"] = 1;
    presentProgram["debugView"] = debugView;
    presentProgram["tilesX"] = tilesX;
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.1f, 0.1f, 0.1f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    float scale = std::min((float)windowWidth / renderWidth, (float)windowHeight / renderHeight);
    int width = int(renderWidth * scale), height = int(renderHeight * scale);
    int x = (windowWidth - width) / 2, y = (windowHeight - height) / 2;
    glBlitNamedFramebuffer(finalFramebuffer, 0, 0, 0, renderWidth, renderHeight, x, y, x + width, y + height,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

// Largest and mean difference between the outputs of the two renderers, in 8-bit color levels.
void compareRenderers(int& largest, double& mean) {
    size_t bytes = (size_t)renderWidth * renderHeight * 4;
    std::vector<unsigned char> tile(bytes), quad(bytes);
    glGetTextureImage(tileImage, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)bytes, tile.data());
    glGetTextureImage(quadImage, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)bytes, quad.data());
    largest = 0;
    mean = 0;
    for (size_t i = 0; i < bytes; i++) {
        int difference = std::abs(tile[i] - quad[i]);
        largest = std::max(largest, difference);
        mean += (double)difference / bytes;
    }
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

    // sort of random keys with many duplicates; a stable sort on the CPU must give the same order
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
