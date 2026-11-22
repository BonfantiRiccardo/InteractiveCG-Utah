#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/freeglut.h>
#include <chrono>
#include <cstdio>
#include "lodepng.h"
#include "renderer.h"

extern "C" { __declspec(dllexport) unsigned long NvOptimusEnablement = 1; }     // laptops with two GPUs: use the NVIDIA one

const char* VIEW_NAMES[VIEW_COUNT] = { "image", "splats per tile (red = 4000)", "difference between the renderers x16",
                                       "normals", "flatness", "light without the captured colors", "surface depth" };
const float FOV_Y = 50.0f * 3.14159265f / 180.0f;

cy::Vec3f sceneCenter, sceneUp;         // estimated by the loader
float     sceneRadius, sceneExtent;
cy::Vec3f target, axisA, axisB;         // orbit center, two horizontal axes perpendicular to sceneUp
float     yaw, pitch, distance;         // orbit angles in radians and distance from the target

RenderOptions options;                  // renderer switches, set from the keys
int    fixedHeight = 0;                // 0 renders at the window size, otherwise 720, 1080 or 1440 lines at 16:9
int    windowWidth = 1280, windowHeight = 720;
int    mouseButton = -1, mouseX, mouseY, pressX, pressY;
bool   mouseOnObjects = false;          // Ctrl was held when the button went down, so the mouse acts on the objects
bool   mouseDragged = false;            // the mouse has moved since the button went down
cy::Vec3f grabOffset;                   // object position minus the grabbed point, so the object does not jump to the cursor
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
    cy::Vec3f position = target + orbitDirection() * distance;
    return perspectiveCamera(position, target, sceneUp, FOV_Y, screen.width, screen.height,
                             (position - sceneCenter).Length() + sceneExtent);
}

// The tile renderer is active when it is selected, and also for the debug views that only it can produce.
bool tilesActive() {
    return options.useTiles || options.debugView == VIEW_TILE_LOAD || options.debugView == VIEW_DEPTH;
}

// Sets the render size: the window size, or a fixed 16:9 size shown scaled in the window.
void applyResolution() {
    if (fixedHeight) resizeRenderer(fixedHeight * 16 / 9, fixedHeight);
    else resizeRenderer(std::max(windowWidth, 1), std::max(windowHeight, 1));
}

// Converts a window pixel (origin at the top left) to a position in the rendered image (origin at the bottom left).
cy::Vec2f windowToRender(int x, int y) {
    float scale = std::min((float)windowWidth / screen.width, (float)windowHeight / screen.height);
    return cy::Vec2f((x + 0.5f - 0.5f * (windowWidth - screen.width * scale)) / scale,
                     (windowHeight - y - 0.5f - 0.5f * (windowHeight - screen.height * scale)) / scale);
}

bool insideImage(cy::Vec2f p) {
    return p.x >= 0 && p.y >= 0 && p.x < screen.width && p.y < screen.height;
}

// Removes the rounding drift of a rotation matrix that has been multiplied many times.
cy::Matrix3f orthonormalize(const cy::Matrix3f& m) {
    cy::Vec3f x = m.Column(0).GetNormalized();
    cy::Vec3f z = (x ^ m.Column(1)).GetNormalized();
    return cy::Matrix3f(x, z ^ x, z);
}

// World-space camera ray through a position of the rendered image. A step of 1 along it adds 1 to the view depth.
cy::Vec3f cameraRay(const Camera& camera, cy::Vec2f p) {
    cy::Vec3f inView((p.x - 0.5f * screen.width) / camera.focal, (p.y - 0.5f * screen.height) / camera.focal, -1);
    return cy::Matrix3f(camera.view).GetTranspose() * inView;
}

// Intersects the camera ray through a window pixel with the horizontal plane through a point. Returns false on a miss.
bool horizontalHit(int x, int y, cy::Vec3f through, cy::Vec3f& hit) {
    Camera camera = currentCamera();
    cy::Vec3f ray = cameraRay(camera, windowToRender(x, y));
    float t = ((through - camera.position) % sceneUp) / (ray % sceneUp);
    if (!(t > 0 && t < sceneExtent)) return false;      // also false for a ray along the plane
    hit = camera.position + ray * t;
    return true;
}

// Adds an object where the mouse points and selects it. A mesh stands on the visible surface and a light floats above it.
// Where there is no surface under the mouse, the object goes at the distance of the orbit center.
void addObject(ObjectType type, int x, int y) {
    if (type != OBJECT_MESH && lightCount() == MAX_LIGHTS) { std::printf("no more than %d lights\n", MAX_LIGHTS); return; }
    Camera camera = currentCamera();
    cy::Vec2f p = windowToRender(x, y);
    if (!insideImage(p)) p = cy::Vec2f(0.5f * screen.width, 0.5f * screen.height);
    float depth = tilesActive() ? readPixel(screen.surfaceImage, int(p.x), int(p.y)).x : 0;
    cy::Vec3f point = camera.position + cameraRay(camera, p) * (depth > 0 ? depth : distance);

    SceneObject object;
    cy::Vec3f right = cy::Matrix3f(camera.view).GetRow(0);     // right axis of the camera, which is horizontal
    object.type = type;
    object.rotation = cy::Matrix3f(right, sceneUp ^ right, sceneUp);    // local x to the right, local z up
    if (type == OBJECT_MESH) {
        object.size = 0.06f * distance;
        object.color = PALETTE_SIZE - 1;
        object.position = point + sceneUp * (shapes[type].base * object.size);
    } else {
        if (lightCount() == 0) ambient = 0.3f;                  // dim the capture so that the new light shows
        object.size = 0.03f * distance;
        object.reach = 0.15f * distance;
        object.position = point + sceneUp * object.reach;
        if (type == OBJECT_DIRECTIONAL_LIGHT)                   // points down, tilted away from the camera
            object.rotation = cy::Matrix3f::Rotation(right, 0.7f) * object.rotation;
    }
    objects.push_back(object);
    selected = (int)objects.size() - 1;
}

void removeSelected() {
    if (selected >= 0) objects.erase(objects.begin() + selected);
    selected = -1;
}

// Ctrl + click selects the object under the mouse. Clicking the selected object or empty space deselects.
void pickObject(int x, int y) {
    cy::Vec2f p = windowToRender(x, y);
    int hit = insideImage(p) ? int(readPixel(screen.meshInfoImage, int(p.x), int(p.y)).y) - 1 : -1;
    selected = hit == selected || hit >= (int)objects.size() ? -1 : hit;
}

// Prints the state of the renderer and the smoothed timings to the console.
void printTimings() {
    std::printf("\n%s renderer, %dx%d, %s bounds\n", tilesActive() ? "tile" : "quad", screen.width, screen.height,
                options.tightBounds ? "tight" : "3 sigma");
    std::printf("%d splats, %u pairs, %.2f ms per frame (%.1f fps)\n", splatCount, framePairs[0], frameMs, 1000.0 / frameMs);
    for (int pass = 0; pass < PASS_COUNT; pass++) std::printf("  %-10s %6.2f ms\n", PASS_NAMES[pass], passMs[pass]);
    std::printf("buffers: %.0f MB\n", gpuBytes / 1048576.0);
    if (GLEW_NVX_gpu_memory_info) {
        GLint totalKb = 0, freeKb = 0;
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalKb);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &freeKb);
        std::printf("GPU memory in use by all programs: %d of %d MB\n", (totalKb - freeKb) / 1024, totalKb / 1024);
    }
    if (options.debugView == VIEW_DIFFERENCE) {
        int largest;
        double mean;
        compareRenderers(screen, largest, mean);
        std::printf("difference between the renderers: largest %d, mean %.4f (of 255)\n", largest, mean);
    }
}

// Saves the rendered image, with its debug view and without the text, as a PNG file.
void saveScreenshot() {
    size_t rowBytes = (size_t)screen.width * 4;
    std::vector<unsigned char> pixels(rowBytes * screen.height), flipped(pixels.size());
    glGetTextureImage(finalImage, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLsizei)pixels.size(), pixels.data());
    for (int y = 0; y < screen.height; y++)
        std::memcpy(&flipped[y * rowBytes], &pixels[(screen.height - 1 - y) * rowBytes], rowBytes);
    char filename[64];
    std::snprintf(filename, sizeof(filename), "screenshot_%03d.png", screenshotCount++);
    lodepng::encode(filename, flipped, screen.width, screen.height);
    std::printf("saved %s\n", filename);
}

// Draws the state, the timings and the main controls in the top left corner of the window.
void drawText() {
    char selection[96] = "nothing";
    if (selected >= 0) {
        const SceneObject& object = objects[selected];
        if (object.type == OBJECT_MESH) std::snprintf(selection, sizeof(selection), "teapot, size %.2f", object.size);
        else if (object.type == OBJECT_SPOT_LIGHT)
            std::snprintf(selection, sizeof(selection), "spot light, intensity %.2f, cone %.0f degrees", object.intensity, object.cone);
        else std::snprintf(selection, sizeof(selection), "%s, intensity %.2f", OBJECT_NAMES[object.type], object.intensity);
    }
    char text[1024];
    std::snprintf(text, sizeof(text),
                  "%s renderer   %dx%d   %s bounds   view: %s\n"
                  "%d splats   %u pairs%s\n"
                  "%.1f ms per frame (%.0f fps)\n"
                  "meshes %.2f   preprocess %.2f   scan %.2f   duplicate %.2f   sort %.2f   ranges %.2f   render %.2f ms\n"
                  "%d objects   ambient %.2f   meshes against the %s of the splats   selected: %s\n"
                  "4 5 6 7: add a teapot, point, spot, directional light at the mouse   Ctrl + click: select\n"
                  "Ctrl + left drag: move   Ctrl + right drag: rotate   Ctrl + wheel: raise   + -: size, intensity   Del: remove",
                  tilesActive() ? "tile" : "quad", screen.width, screen.height, options.tightBounds ? "tight" : "3 sigma",
                  VIEW_NAMES[options.debugView],
                  splatCount, framePairs[0], framePairs[1] > framePairs[0] ? " (more do not fit in memory)" : "",
                  frameMs, 1000.0 / frameMs,
                  passMs[0], passMs[1], passMs[2], passMs[3], passMs[4], passMs[5], passMs[6],
                  (int)objects.size(), ambient, options.peakDepth ? "peak" : "center", selection);
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

    // read the results of the previous frame and grow the sort buffers if its pairs did not fit
    updateTimers();
    if (frameIndex > 0) glGetNamedBufferSubData(frameInfoBuffer, 0, sizeof(framePairs), framePairs);
    if (framePairs[1] > pairCapacity && pairCapacity < MAX_PAIRS)
        allocateSortBuffers((GLuint)std::min<uint64_t>(MAX_PAIRS, (uint64_t)framePairs[1] * 5 / 4));

    trackSceneChanges();
    Camera camera = currentCamera();
    RenderOptions shown = options;         // options of this frame: the active renderer, the selection and the depth scale
    shown.useTiles = tilesActive();
    shown.selectedId = selected + 1;
    shown.depthRange = 2 * distance;
    uploadLights();
    renderMeshes(camera, screen, sceneUp, shown);
    if (shown.debugView == VIEW_DIFFERENCE) {     // the other renderer first, without timers
        RenderOptions other = shown;
        other.useTiles = !shown.useTiles;
        timingEnabled = false;
        renderSplats(camera, screen, other);
        timingEnabled = true;
    }
    renderSplats(camera, screen, shown);
    present(shown, windowWidth, windowHeight);
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
    SceneObject* object = selected >= 0 ? &objects[selected] : nullptr;
    switch (key) {
        case 27: glutLeaveMainLoop(); break;
        case '\t': options.useTiles = !options.useTiles; break;
        case 'v': options.debugView = (options.debugView + 1) % VIEW_COUNT; break;
        case 'V': options.debugView = (options.debugView + VIEW_COUNT - 1) % VIEW_COUNT; break;
        case 'b': case 'B': options.tightBounds = !options.tightBounds; break;
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
        case '4': case '5': case '6': case '7': addObject(ObjectType(key - '4'), x, y); break;
        case 127: case 8: removeSelected(); break;
        case '+': case '=': if (object) (object->type == OBJECT_MESH ? object->size : object->intensity) *= 1.1f; break;
        case '-': if (object) (object->type == OBJECT_MESH ? object->size : object->intensity) /= 1.1f; break;
        case 'x': case 'X': if (object) object->cone = std::min(object->cone + 2, 80.0f); break;
        case 'z': case 'Z': if (object) object->cone = std::max(object->cone - 2, 4.0f); break;
        case 'c': case 'C': if (object) object->color = (object->color + 1) % PALETTE_SIZE; break;
        case 'k': case 'K': ambient = std::round(std::min(ambient + 0.05f, 2.0f) * 20) / 20; break;
        case 'j': case 'J': ambient = std::round(std::max(ambient - 0.05f, 0.0f) * 20) / 20; break;
        case 'o': case 'O': options.peakDepth = !options.peakDepth; break;
        case 'g': case 'G': showLightShapes = !showLightShapes; break;
    }
}

void mySpecialKeys(int key, int x, int y) {
    if (key == GLUT_KEY_F2) screenshotRequested = true;
    if (key == GLUT_KEY_F6) std::printf(buildPrograms() ? "shaders reloaded\n" : "shader errors\n");
    if (key == GLUT_KEY_DELETE) removeSelected();
    if (selected >= 0 && key == GLUT_KEY_PAGE_UP) objects[selected].position += sceneUp * (0.01f * distance);
    if (selected >= 0 && key == GLUT_KEY_PAGE_DOWN) objects[selected].position -= sceneUp * (0.01f * distance);
}

// With Ctrl the mouse acts on the objects (click to select, drag to move or rotate). Without Ctrl it moves the camera.
void myMouse(int button, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        mouseButton = button;
        mouseOnObjects = (glutGetModifiers() & GLUT_ACTIVE_CTRL) != 0;
        mouseDragged = false;
        pressX = x;
        pressY = y;
        cy::Vec3f hit;
        grabOffset.Zero();
        if (mouseOnObjects && selected >= 0 && horizontalHit(x, y, objects[selected].position, hit))
            grabOffset = objects[selected].position - hit;
    } else {
        if (mouseOnObjects && !mouseDragged && button == GLUT_LEFT_BUTTON) pickObject(x, y);
        mouseButton = -1;
    }
    mouseX = x;
    mouseY = y;
}

void myMotion(int x, int y) {
    float dx = float(x - mouseX), dy = float(y - mouseY);
    if (std::abs(x - pressX) + std::abs(y - pressY) > 3) mouseDragged = true;
    if (mouseOnObjects && selected >= 0 && mouseDragged) {
        SceneObject& object = objects[selected];
        cy::Vec3f hit;
        if (mouseButton == GLUT_LEFT_BUTTON && horizontalHit(x, y, object.position, hit)) object.position = hit + grabOffset;
        if (mouseButton == GLUT_RIGHT_BUTTON) {     // around the up axis and around the right axis of the camera
            cy::Vec3f right = cy::Matrix3f(currentCamera().view).GetRow(0);
            object.rotation = orthonormalize(cy::Matrix3f::Rotation(sceneUp, 0.01f * dx) * cy::Matrix3f::Rotation(right, 0.01f * dy) *
                                             object.rotation);
        }
    }
    if (!mouseOnObjects && mouseButton == GLUT_LEFT_BUTTON) {
        yaw -= 0.005f * dx;
        pitch = std::clamp(pitch + 0.005f * dy, -1.5f, 1.5f);
    }
    if (!mouseOnObjects && mouseButton == GLUT_RIGHT_BUTTON) distance *= std::exp(0.005f * dy);
    mouseX = x;
    mouseY = y;
}

// The wheel zooms. With Ctrl it raises or lowers the selected object.
void myMouseWheel(int wheel, int direction, int x, int y) {
    if ((glutGetModifiers() & GLUT_ACTIVE_CTRL) && selected >= 0) objects[selected].position += sceneUp * (0.02f * distance * direction);
    else distance *= std::exp(-0.1f * direction);
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
    std::printf("Camera: left drag orbits, right drag or wheel zooms, WASD QE move, R reset.\n"
                "Renderer: Tab renderer, V and Shift+V view, B bounds, 0-3 resolution, P timings, T self-test, F2 screenshot,\n"
                "F6 reload shaders, Esc quit.\n"
                "Objects: 4 teapot, 5 point light, 6 spot light, 7 directional light, added where the mouse points.\n"
                "Ctrl + click selects or deselects. For the selected object: Ctrl + left drag moves it horizontally,\n"
                "Ctrl + right drag rotates it, Ctrl + wheel or Page Up/Down raises it, + and - change the size of a mesh or the\n"
                "intensity of a light, Z and X the cone of a spot light, C the color, Delete removes it.\n"
                "J and K change the ambient scale, O the depth at which splats meet meshes, G hides the light shapes.\n");

    //Register GLUT callbacks
    glutDisplayFunc(myDisplay);
    glutIdleFunc(myIdle);
    glutReshapeFunc(myReshape);
    glutKeyboardFunc(myKeyboard);
    glutSpecialFunc(mySpecialKeys);
    glutMouseFunc(myMouse);
    glutMotionFunc(myMotion);
    glutMouseWheelFunc(myMouseWheel);
    glutMainLoop();                 //Enter the main loop
    return 0;
}
