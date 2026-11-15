#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/freeglut.h>
#include <chrono>
#include <cstdio>
#include "lodepng.h"
#include "renderer.h"

extern "C" { __declspec(dllexport) unsigned long NvOptimusEnablement = 1; }     // laptops with two GPUs: use the NVIDIA one

enum DebugView { VIEW_IMAGE, VIEW_TILE_LOAD, VIEW_DIFFERENCE, VIEW_COUNT };
const char* VIEW_NAMES[VIEW_COUNT] = { "image", "splats per tile (red = 4000)", "difference between the renderers x16" };
const float FOV_Y = 50.0f * 3.14159265f / 180.0f;

cy::Vec3f sceneCenter, sceneUp;         // estimated by the loader
float     sceneRadius, sceneExtent;
cy::Vec3f target, axisA, axisB;         // orbit center, two horizontal axes perpendicular to sceneUp
float     yaw, pitch, distance;         // orbit angles in radians and distance from the target

bool   useTiles = true;                 // tile renderer (compute) or quad renderer (rasterization)
bool   tightBounds = true;
int    debugView = VIEW_IMAGE;
int    fixedHeight = 0;                 // 0: render at the window size; otherwise 720, 1080 or 1440 lines at 16:9
int    windowWidth = 1280, windowHeight = 720;
int    mouseButton = -1, mouseX, mouseY;
GLuint framePairs[2];                   // pairs sorted and pairs requested in the previous frame
double frameMs = 0;                     // smoothed time between frames
int    screenshotCount = 0;
bool   screenshotRequested = false;

// Puts the camera above the scene center, looking down at 30 degrees.
void resetCamera() {
    target = sceneCenter;
    yaw = 0;
    pitch = 30.0f * 3.14159265f / 180.0f;
    distance = 0.5f * sceneRadius;
}

// Direction from the orbit target to the camera.
cy::Vec3f orbitDirection() {
    return (axisA * std::cos(yaw) + axisB * std::sin(yaw)) * std::cos(pitch) + sceneUp * std::sin(pitch);
}

// Camera of the current frame.
Camera currentCamera() {
    Camera camera;
    camera.position = target + orbitDirection() * distance;
    camera.view = cy::Matrix4f::View(camera.position, target, sceneUp);
    camera.focal = 0.5f * renderHeight / std::tan(0.5f * FOV_Y);
    camera.zFar = (camera.position - sceneCenter).Length() + sceneExtent;
    return camera;
}

// The tile renderer is active when it is selected, and also while its debug view is shown.
bool tilesActive() {
    return useTiles || debugView == VIEW_TILE_LOAD;
}

// Sets the render size: the window size, or a fixed 16:9 size shown scaled in the window.
void applyResolution() {
    if (fixedHeight) resizeRenderer(fixedHeight * 16 / 9, fixedHeight);
    else resizeRenderer(std::max(windowWidth, 1), std::max(windowHeight, 1));
}

// Prints the state of the renderer and the smoothed timings to the console.
void printTimings() {
    std::printf("\n%s renderer, %dx%d, %s bounds\n", tilesActive() ? "tile" : "quad", renderWidth, renderHeight,
                tightBounds ? "tight" : "3 sigma");
    std::printf("%d splats, %u pairs, %.2f ms per frame (%.1f fps)\n", splatCount, framePairs[0], frameMs, 1000.0 / frameMs);
    for (int pass = 0; pass < PASS_COUNT; pass++) std::printf("  %-10s %6.2f ms\n", PASS_NAMES[pass], passMs[pass]);
    std::printf("buffers: %.0f MB\n", gpuBytes / 1048576.0);
    if (GLEW_NVX_gpu_memory_info) {
        GLint totalKb = 0, freeKb = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalKb);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &freeKb);
        std::printf("GPU memory in use by all programs: %d of %d MB\n", (totalKb - freeKb) / 1024, totalKb / 1024);
    }
    if (debugView == VIEW_DIFFERENCE) {
        int largest;
        double mean;
        compareRenderers(largest, mean);
        std::printf("difference between the renderers: largest %d, mean %.4f (of 255)\n", largest, mean);
    }
}

// Saves the rendered image, with its debug view and without the text, as a PNG file.
void saveScreenshot() {
    size_t rowBytes = (size_t)renderWidth * 4;
    std::vector<unsigned char> pixels(rowBytes * renderHeight), flipped(pixels.size());
    glGetTextureImage(finalImage, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)pixels.size(), pixels.data());
    for (int y = 0; y < renderHeight; y++)
        std::memcpy(&flipped[y * rowBytes], &pixels[(renderHeight - 1 - y) * rowBytes], rowBytes);
    char filename[64];
    std::snprintf(filename, sizeof(filename), "screenshot_%03d.png", screenshotCount++);
    lodepng::encode(filename, flipped, renderWidth, renderHeight);
    std::printf("saved %s\n", filename);
}

// Draws the state and the timings in the top left corner of the window.
void drawText() {
    char text[512];
    std::snprintf(text, sizeof(text),
                  "%s renderer   %dx%d   %s bounds   view: %s\n"
                  "%d splats   %u pairs%s\n"
                  "%.1f ms per frame (%.0f fps)\n"
                  "preprocess %.2f   scan %.2f   duplicate %.2f   sort %.2f   ranges %.2f   render %.2f ms",
                  tilesActive() ? "tile" : "quad", renderWidth, renderHeight, tightBounds ? "tight" : "3 sigma", VIEW_NAMES[debugView],
                  splatCount, framePairs[0], framePairs[1] > framePairs[0] ? " (more do not fit in memory)" : "",
                  frameMs, 1000.0 / frameMs,
                  passMs[0], passMs[1], passMs[2], passMs[3], passMs[4], passMs[5]);
    glUseProgram(0);
    for (int shadow = 1; shadow >= 0; shadow--) {      // black text one pixel off, then white text on top
        glColor3f(1.0f - shadow, 1.0f - shadow, 1.0f - shadow);
        glWindowPos2i(10 + shadow, windowHeight - 20 - shadow);
        glutBitmapString(GLUT_BITMAP_8_BY_13, (const unsigned char*)text);
    }
}

// My display callback: renders a frame, presents it, draws the text and swaps the buffers.
void myDisplay() {
    static auto lastTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    frameMs += 0.1 * (std::chrono::duration<double, std::milli>(now - lastTime).count() - frameMs);
    lastTime = now;

    // results of the previous frame; the sort buffers grow when its pairs did not fit
    updateTimers();
    if (frameIndex > 0) glGetNamedBufferSubData(frameInfoBuffer, 0, sizeof(framePairs), framePairs);
    if (framePairs[1] > pairCapacity && pairCapacity < MAX_PAIRS)
        allocateSortBuffers((GLuint)std::min<uint64_t>(MAX_PAIRS, (uint64_t)framePairs[1] * 5 / 4));

    Camera camera = currentCamera();
    if (debugView == VIEW_DIFFERENCE) {     // the other renderer first, without timers
        timingEnabled = false;
        renderSplats(camera, !tilesActive(), tightBounds);
        timingEnabled = true;
    }
    renderSplats(camera, tilesActive(), tightBounds);
    present(tilesActive(), debugView, windowWidth, windowHeight);
    if (screenshotRequested) saveScreenshot();
    screenshotRequested = false;
    drawText();
    glutSwapBuffers();
    frameIndex++;
}

void myIdle() {
    glutPostRedisplay();
}

void myReshape(int width, int height) {
    windowWidth = width;
    windowHeight = height;
    applyResolution();
}

void myKeyboard(unsigned char key, int x, int y) {
    cy::Vec3f forward = (axisA * std::cos(yaw) + axisB * std::sin(yaw)) * -1.0f;    // horizontal, away from the camera
    cy::Vec3f right = forward ^ sceneUp;
    float step = 0.05f * distance;
    switch (key) {
        case 27: glutLeaveMainLoop(); break;
        case '\t': useTiles = !useTiles; break;
        case 'v': case 'V': debugView = (debugView + 1) % VIEW_COUNT; break;
        case 'b': case 'B': tightBounds = !tightBounds; break;
        case 'p': case 'P': printTimings(); break;
        case 't': case 'T': selfTest(); break;
        case 'r': case 'R': resetCamera(); break;
        case '0': fixedHeight = 0; applyResolution(); break;
        case '1': fixedHeight = 720; applyResolution(); break;
        case '2': fixedHeight = 1080; applyResolution(); break;
        case '3': fixedHeight = 1440; applyResolution(); break;
        case 'w': case 'W': target += forward * step; break;
        case 's': case 'S': target -= forward * step; break;
        case 'd': case 'D': target += right * step; break;
        case 'a': case 'A': target -= right * step; break;
        case 'e': case 'E': target += sceneUp * step; break;
        case 'q': case 'Q': target -= sceneUp * step; break;
    }
}

void mySpecialKeys(int key, int x, int y) {
    if (key == GLUT_KEY_F2) screenshotRequested = true;
    if (key == GLUT_KEY_F6) std::printf(buildPrograms() ? "shaders reloaded\n" : "shader errors\n");
}

void myMouse(int button, int state, int x, int y) {
    mouseButton = state == GLUT_DOWN ? button : -1;
    mouseX = x;
    mouseY = y;
}

void myMotion(int x, int y) {
    if (mouseButton == GLUT_LEFT_BUTTON) {
        yaw -= 0.005f * (x - mouseX);
        pitch = std::clamp(pitch + 0.005f * (y - mouseY), -1.5f, 1.5f);
    }
    if (mouseButton == GLUT_RIGHT_BUTTON) distance *= std::exp(0.005f * (y - mouseY));
    mouseX = x;
    mouseY = y;
}

// Prints the OpenGL errors.
void GLAPIENTRY myDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
                               const void* user) {
    if (type == GL_DEBUG_TYPE_ERROR) std::cerr << "OpenGL error: " << message << std::endl;
}

int main(int argc, char** argv) {
    // Load the model either from command line or default path and initialize camera.
    const char* modelFile = argc > 1 ? argv[1] : "../models/garden.ply";
    std::printf("Loading %s\n", modelFile);
    SplatData splats;
    if (!loadPly(modelFile, splats)) return 1;
    sceneCenter = splats.center;
    sceneUp = splats.up;
    sceneRadius = splats.radius;
    sceneExtent = splats.extent;
    axisA = (sceneUp ^ (std::abs(sceneUp.x) < 0.9f ? cy::Vec3f(1, 0, 0) : cy::Vec3f(0, 1, 0))).GetNormalized();
    axisB = sceneUp ^ axisA;
    resetCamera();

    // Initialize OpenGL, create the window and the renderer, and enter the main loop.
    glutInit(&argc, argv);
    glutInitContextVersion(4, 6);
    glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);     // the text is drawn with fixed-function OpenGL
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Final project");
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW failed" << std::endl; return 1; }
    std::printf("%s, OpenGL %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(myDebugMessage, nullptr);
    if (WGLEW_EXT_swap_control) wglSwapIntervalEXT(0);      // no vsync: the frame time shows the real cost

    if (!initRenderer(splats)) return 1;
    splats = SplatData();                                   // the model now lives on the GPU
    applyResolution();
    std::printf("%d splats, %.0f MB of buffers\n", splatCount, gpuBytes / 1048576.0);
    std::printf("Mouse: left drag orbits, right drag zooms. Keys: WASD QE move, R reset, Tab renderer, V view, B bounds,\n"
                "0-3 resolution, P timings, T self-test, F2 screenshot, F6 reload shaders, Esc quit.\n");

    //Register GLUT callbacks
    glutDisplayFunc(myDisplay);
    glutIdleFunc(myIdle);
    glutReshapeFunc(myReshape);
    glutKeyboardFunc(myKeyboard);
    glutSpecialFunc(mySpecialKeys);
    glutMouseFunc(myMouse);
    glutMotionFunc(myMotion);
    glutMainLoop();                 //Enter the main loop
    return 0;
}
