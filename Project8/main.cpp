#include <GL/glew.h>
#include <GL/freeglut.h>

#include <cyMatrix.h>
#include <cyTriMesh.h>
#include <cyGL.h>

#include "lodepng.h"

#include <iostream>

//Camera control variables
#define FAR_PLANE 100.0f
#define NEAR_PLANE 0.1f
#define FOV 45.0f

//Material Properties
#define K_A cy::Vec3f(0.2f, 0.2f, 0.2f)     // Grey
#define K_D cy::Vec3f(1.0f, 1.0f, 1.0f)     // White
#define K_S cy::Vec3f(1.0f, 1.0f, 1.0f)     // White specular highlight
#define SHININESS 32.0f

#define SPOT_FOV 60.0f // Spotlight field of view in degrees

// Global variables
cy::TriMesh pointLightMesh;
cy::GLSLProgram lightProgram, planeProgram, wireProgram, tessProgram, tessWireProgram, shadowProgram;
GLuint lightVAO, planeVAO;

cy::GLRenderDepth2D shadowMap; // Depth buffer for shadow mapping
cy::GLTexture2D normalMap, displacementMap; // Normal and displacement maps for the plane
bool hasDisplacementMap = false; // Flag to indicate if a displacement map is provided
bool showWireframe = false; // Flag to toggle wireframe rendering

float tessLevel = 8.0f; // Default tessellation level
float tessDisplacementScale = 0.3f; // Default displacement scale

//Camera variables
float cameraAngleX = 0.0f;
float cameraAngleY = 0.0f;
float cameraDistance = 5.0f;

// Camera control variables
cy::Vec2f rightMouseStartDrag;
bool isRightMouseButtonDown = false;
cy::Vec2f leftMouseStartDrag;
bool isLeftMouseButtonDown = false;
float aspectRatio = 800.0f / 600.0f;

bool isPerspective = true; // Flag to toggle between perspective and orthogonal projection

// Light Properties
float lightAngleX = 0.0f;
float lightAngleY = 0.0f;
float lightDistance = 2.0f;
bool isDraggingLight = false; // Flag to indicate if the light is being dragged

// ------------------------------ Supporting Functions -----------------------------
int loadTexture(const std::string& texturePath, cyGLTexture2D& texture) {
    std::vector<unsigned char> image; //the raw pixels
    unsigned int width, height;

    // Decode the PNG file using LodePNG
    if (lodepng::decode(image, width, height, texturePath)) {
        std::cerr << "Failed to decode PNG file: " << texturePath << std::endl;
        return -1;
    }

    // Flip the image vertically to match OpenGL's coordinate system
    std::vector<unsigned char> flippedImage(image.size());
    for (unsigned int i = 0; i < height; i++) {
        for (unsigned int j = 0; j < width; j++) {
            unsigned int srcIndex = (height - 1 - i) * width * 4 + j * 4;
            unsigned int dstIndex = i * width * 4 + j * 4;
            flippedImage[dstIndex] = image[srcIndex];
            flippedImage[dstIndex + 1] = image[srcIndex + 1];
            flippedImage[dstIndex + 2] = image[srcIndex + 2];
            flippedImage[dstIndex + 3] = image[srcIndex + 3];
        }
    }

    // Initialize the texture with cyGL
    texture.Initialize();
    // Set the image for the texture
    texture.SetImage(flippedImage.data(), 4, width, height);
    texture.BuildMipmaps();
    texture.SetWrappingMode(GL_REPEAT, GL_REPEAT);
    texture.SetFilteringMode(GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);

    return 0;
}

// ------------------------------ Rendering Functions ------------------------------
void myDisplay() {
    // ------------------------------ Update Camera and Light Matrices -----------------------------
    //Rebuild the MVP matrix based on the current camera parameters
    cy::Matrix4f view = cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, -cameraDistance)) * cy::Matrix4f::RotationX(cameraAngleX) * cy::Matrix4f::RotationZ(cameraAngleY);

    cy::Matrix4f projection;
    if (isPerspective) {
        projection = cy::Matrix4f::Perspective(FOV * cy::Pi<float>() / 180.0f, aspectRatio, NEAR_PLANE, FAR_PLANE);
    } else {
        // Orthographic projection
        float sy = 1.0f / (cameraDistance * tan(FOV * cy::Pi<float>() /180 /2));
        float sx = sy / aspectRatio;
        float sz = -2.0f / (FAR_PLANE - NEAR_PLANE);
        float zOffset = -(FAR_PLANE + NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE);
        projection = cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, zOffset)) * cy::Matrix4f::Scale(sx, sy, sz);
    }

    cy::Matrix4f lightFrame = cy::Matrix4f::RotationZ(lightAngleY) * cy::Matrix4f::RotationX(lightAngleX) * cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, lightDistance));
    cy::Vec3f lightWorld = lightFrame.GetTranslation();
    cy::Vec3f lightEye = (view * cy::Vec4f(lightWorld, 1.0f)).XYZ();
    // Compute light view matrix for shadow mapping
    cy::Matrix4f lightView = lightFrame.GetInverse();
    cy::Matrix4f lightProjection = cy::Matrix4f::Perspective(SPOT_FOV * cy::Pi<float>() / 180.0f, 1.0f, NEAR_PLANE, FAR_PLANE);
    // Bias matrix to transform from [-1,1] to [0,1]
    cy::Matrix4f shadowBias = cy::Matrix4f::Translation(cy::Vec3f(0.5f, 0.5f, 0.5f)) * cy::Matrix4f::Scale(0.5f, 0.5f, 0.5f);
    cy::Vec3f spotDir = view.GetSubMatrix3() * (-lightWorld).GetNormalized();     // Transform the spotlight direction to eye space
    float spotCutoff = cos(SPOT_FOV * 0.5f * cy::Pi<float>() / 180.0f); // Compute the cosine of the cutoff angle for the spotlight


    //Plane matrices
    cy::Matrix4f planeMVP = projection * view;

    // ------------------------------ Render to shadow map ------------------------------------
    shadowProgram.Bind();
    shadowMap.Bind(); // Bind the depth buffer for off-screen rendering
    glClear(GL_DEPTH_BUFFER_BIT); // Clear the depth buffer

    if (hasDisplacementMap) {
        glEnable(GL_POLYGON_OFFSET_FILL); // Enable polygon offset to avoid shadow acne
        glPolygonOffset(2.0f, 4.0f); // Set the polygon offset parameters

        shadowProgram["mvp"] = lightProjection * lightView;
        shadowProgram["tessLevel"] = tessLevel; // Set the tessellation level
        shadowProgram["displacementScale"] = tessDisplacementScale; // Set the displacement scale
        shadowProgram["displacementMap"] = 1; // Set the shader uniform to use texture unit 1
        displacementMap.Bind(1); // Bind the displacement map to texture unit 1

        glBindVertexArray(planeVAO);
        glPatchParameteri(GL_PATCH_VERTICES, 4); // Set the number of vertices per patch for tessellation
        glDrawArrays(GL_PATCHES, 0, 4); // Draw the plane with tessellation
        
        glDisable(GL_POLYGON_OFFSET_FILL); // Disable polygon offset after rendering
    }
    shadowMap.Unbind(); // Unbind the depth buffer after rendering

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the default framebuffer

    // ------------------------------ Draw main scene ------------------------------------
    // Not needed for this project

    //------------------------------ Draw the light source ------------------------------------
    // Draw the light source as a small point
    lightProgram.Bind();
    // Scale down the light source mesh and position it at the light's world position
    lightProgram["mvp"] = projection * view * lightFrame * cy::Matrix4f::Scale(0.1f);

    glBindVertexArray(lightVAO);
    glDrawArrays(GL_TRIANGLES, 0, pointLightMesh.NF() * 3); // Draw the light source mesh

    // ------------------------------ Draw the plane ------------------------------------
    
    // Draw the plane
    cy::GLSLProgram& program = (hasDisplacementMap) ? tessProgram : planeProgram; // Choose the appropriate shader program based on the presence of a displacement map
    program.Bind();
    program["mvp"] = planeMVP;
    program["mvNormal"] = view.GetSubMatrix3().GetInverse().GetTranspose();
    program["modelView"] = view;
    program["lightPos"] = lightEye;
    program["isPerspective"] = isPerspective ? 1 : 0;
    program["shadowMatrix"] = shadowBias * lightProjection * lightView;
    program["K_A"] = K_A;
    program["K_D"] = K_D;
    program["K_S"] = K_S;
    program["shininess"] = SHININESS;
    program["normalMap"] = 0; // Set the shader uniform to use texture unit 0
    normalMap.Bind(0); // Bind the normal map to texture unit 0

    program["shadowMap"] = 3; // Set the shader uniform to use texture unit 3
    shadowMap.BindTexture(3); // Bind the shadow map to texture unit 3

    program["spotLightDir"] = spotDir; // Set the spotlight direction in eye space
    program["spotLightCutoff"] = spotCutoff; // Set the spotlight cutoff angle

    program["tessLevel"] = tessLevel; // Set the tessellation level
    program["displacementScale"] = tessDisplacementScale; // Set the displacement scale
    program["displacementMap"] = 1; // Set the shader uniform to use texture unit 1
    if (hasDisplacementMap) {
        displacementMap.Bind(1); // Bind the displacement map to texture unit 1
    }

    glBindVertexArray(planeVAO);
    if (hasDisplacementMap) {
        glPatchParameteri(GL_PATCH_VERTICES, 4); // Set the number of vertices per patch for tessellation
        glDrawArrays(GL_PATCHES, 0, 4); // Draw the plane
    } else 
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // Draw the plane

    // ------------------------------ Draw wireframe if enabled ------------------------------------
    if (showWireframe) {
        cy::GLSLProgram& chosenWireProgram = (hasDisplacementMap) ? tessWireProgram : wireProgram; // Choose the appropriate wireframe shader program
        chosenWireProgram.Bind();
        chosenWireProgram["mvp"] = planeMVP;
        chosenWireProgram["tessLevel"] = tessLevel; // Set the tessellation level for wireframe rendering
        chosenWireProgram["displacementScale"] = tessDisplacementScale; // Set the displacement
        chosenWireProgram["displacementMap"] = 1; // Set the shader uniform to use texture unit 1
        if (hasDisplacementMap) {
            displacementMap.Bind(1); // Bind the displacement map to texture unit 1
        }

        glBindVertexArray(planeVAO);
        glDrawArrays(hasDisplacementMap ? GL_PATCHES : GL_TRIANGLE_FAN, 0, 4); // Draw the plane in wireframe
    }

    glutSwapBuffers();
}

void myKeyboard(unsigned char key, int x, int y){
    switch (key) {
        case 27: // ESC key
            std::cout << "Exiting the program." << std::endl;
            exit(0);
            break;

        case 'p': case 'P': // Toggle projection mode
            isPerspective = !isPerspective;
            std::cout << "Projection switched to: " << (isPerspective ? "Perspective" : "Orthographic") << std::endl;
            glutPostRedisplay(); // Request a redraw
            break;

        case ' ': // Toggle wireframe rendering
            showWireframe = !showWireframe;
            std::cout << "Wireframe rendering switched to: " << (showWireframe ? "On" : "Off") << std::endl;
            glutPostRedisplay(); // Request a redraw
            break;
    }
}

void mySpecialKeys(int key, int x, int y) {
    switch (key) {    
        case GLUT_KEY_F6: // F6 key
            // Reload the shaders
            if (!lightProgram.BuildFiles("shaders/lightShader.vert", "shaders/lightShader.frag") ||
                !planeProgram.BuildFiles("shaders/planeShader.vert", "shaders/planeShader.frag") ||
                !shadowProgram.BuildFiles("shaders/tessShader.vert", "shaders/shadowShader.frag", nullptr, "shaders/tessShader.tesc", "shaders/tessShader.tese") ||
                !wireProgram.BuildFiles("shaders/planeShader.vert", "shaders/wireShader.frag", "shaders/wireShader.geom") ||
                !tessProgram.BuildFiles("shaders/tessShader.vert", "shaders/planeShader.frag", nullptr, "shaders/tessShader.tesc", "shaders/tessShader.tese") ||
                !tessWireProgram.BuildFiles("shaders/tessShader.vert", "shaders/wireShader.frag", "shaders/wireShader.geom", "shaders/tessShader.tesc", "shaders/tessShader.tese"))
                std::cerr << "Failed to build shader program." << std::endl;
            else
                std::cout << "Shaders reloaded." << std::endl;
            break;

        case GLUT_KEY_UP: // Up arrow key
            tessLevel = std::min(tessLevel + 1.0f, 64.0f); // Increase tessellation level
            std::cout << "Tessellation level increased to: " << tessLevel << std::endl;
            break;
        case GLUT_KEY_DOWN: // Down arrow key
            tessLevel = std::max(1.0f, tessLevel - 1.0f); // Decrease tessellation level, but not below 1
            std::cout << "Tessellation level decreased to: " << tessLevel << std::endl;
            break;
    }
    
    glutPostRedisplay(); // Request a redraw
}

void myMouse(int button, int state, int x, int y) {
    // Handle mouse input here
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        isDraggingLight = (glutGetModifiers() & GLUT_ACTIVE_CTRL) != 0; // Start dragging the light

        isLeftMouseButtonDown = true;
        leftMouseStartDrag = cy::Vec2f(x, y);
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        isLeftMouseButtonDown = false;
        isDraggingLight = false; // Stop dragging the light
    } 
    if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        isRightMouseButtonDown = true;
        rightMouseStartDrag = cy::Vec2f(x, y);
    } else if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {
        isRightMouseButtonDown = false;
    }
}

void myMouseMotion(int x, int y) {
    // Handle mouse motion here
    if (isLeftMouseButtonDown) {
        // Update camera angles based on mouse movement
        cy::Vec2f currentMousePos(x, y);
        cy::Vec2f delta = currentMousePos - leftMouseStartDrag;

        if (isDraggingLight) {
            lightAngleX += delta.y * 0.01f; // Movement along the y-axis affects the x-angle (pitch)
            lightAngleY += delta.x * 0.01f; // Movement along the x-axis affects the y-angle (yaw)

            leftMouseStartDrag = currentMousePos; // Update the last mouse position
            
        } else {

            cameraAngleX += delta.y * 0.01f; // Movement along the y-axis affects the x-angle (pitch)
            cameraAngleY += delta.x * 0.01f; // Movement along the x-axis affects the y-angle (yaw)

            leftMouseStartDrag = currentMousePos; // Update the last mouse position
        }
    }
    if (isRightMouseButtonDown) {
        // Update camera position based on mouse movement
        cy::Vec2f currentMousePos(x, y);
        cy::Vec2f delta = currentMousePos - rightMouseStartDrag;

        if (cameraDistance > FAR_PLANE)
            cameraDistance = FAR_PLANE;
        else if (cameraDistance < NEAR_PLANE)
            cameraDistance = NEAR_PLANE;
        else
            cameraDistance += delta.y * 0.01f; //Movement along the y-axis affects the distance


        rightMouseStartDrag = currentMousePos; // Update the last mouse position
    }

    glutPostRedisplay(); // Request a redraw
}

void myReshape(int width, int height) {
    //Set the viewport to cover the new window size
    glViewport(0, 0, width, height);

    // Update the aspect ratio
    aspectRatio = static_cast<float>(width) / static_cast<float>(height);

}


// ------------------------------ Main Function ------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_normal_map> [path_to_displacement_map]" << std::endl;
        return -1;
    }

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Project8 OpenGL Window");

    // Initialize GLEW
    GLenum res = glewInit();
    if (res != GLEW_OK) {
        std::cerr << "Error: '" << glewGetErrorString(res) << "'" << std::endl;
        return 1;
    }

    //------------------------------ Main mesh setup ---------------------------------
    // Not needed in this project

    // ------------------------------ Textures Setup ------------------------------------
    shadowMap.Initialize(true, 1024, 1024); // Initialize the depth buffer for shadow mapping
    shadowMap.SetTextureFilteringMode(GL_LINEAR, GL_LINEAR); // Set filtering mode for the depth buffer

    // Load normal and displacement maps for the plane
    if (loadTexture(argv[1], normalMap) != 0) {
        std::cerr << "Failed to load normal map." << std::endl;
        return -1;
    }
    if (argc >= 3) {
        if (loadTexture(argv[2], displacementMap) != 0) {
            std::cerr << "Failed to load displacement map." << std::endl;
            return -1;
        }
        hasDisplacementMap = true; // Set the flag to true if a displacement map is provided
    } else {
        hasDisplacementMap = false; // No displacement map provided
        std::cout << "No displacement map provided. Displacement mapping will be disabled." << std::endl;
    }

    // ------------------------------ Plane Setup ------------------------------------
    //Create vertices for the plane
    std::vector<cy::Vec3f> planeVertices = {
        cy::Vec3f(-1.0f, -1.0f, 0.0f), // Bottom-left
        cy::Vec3f(1.0f, -1.0f, 0.0f),  // Bottom-right
        cy::Vec3f(1.0f, 1.0f, 0.0f),   // Top-right
        cy::Vec3f(-1.0f, 1.0f, 0.0f)   // Top-left
    };

    // Generate plane VAO and VBO
    GLuint planeVBO;

    glGenVertexArrays(1, &planeVAO);
    glBindVertexArray(planeVAO);

    glGenBuffers(1, &planeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, planeVertices.size() * sizeof(cy::Vec3f), planeVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);


    if(!planeProgram.BuildFiles("shaders/planeShader.vert", "shaders/planeShader.frag")) {
        std::cerr << "Failed to build plane shader program." << std::endl;
        return -1;
    }
    planeProgram.Bind();


    // ------------------------------ Light Setup ------------------------------------
    if (!pointLightMesh.LoadFromFileObj("light/light.obj")) {
        std::cerr << "Failed to load point light mesh." << std::endl;
        return -1;
    }

    std::vector<cy::Vec3f> lightVertices;
    for (unsigned int i = 0; i < pointLightMesh.NF(); i++) {
        lightVertices.push_back(pointLightMesh.V(pointLightMesh.F(i).v[0]));
        lightVertices.push_back(pointLightMesh.V(pointLightMesh.F(i).v[1]));
        lightVertices.push_back(pointLightMesh.V(pointLightMesh.F(i).v[2]));
    }

    // Generate light object VAO and VBO
    GLuint lightVBO;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);

    glGenBuffers(1, &lightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
    
    glBufferData(GL_ARRAY_BUFFER, lightVertices.size() * sizeof(cy::Vec3f), lightVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    
    // Build and compile the light shader program
    if(!lightProgram.BuildFiles("shaders/lightShader.vert", "shaders/lightShader.frag")) {
        std::cerr << "Failed to build light shader program." << std::endl;
        return -1;
    }
    lightProgram.Bind();



    //------------------------------- Geometry Shader Setup ------------------------------
    if(!wireProgram.BuildFiles("shaders/planeShader.vert", "shaders/wireShader.frag", "shaders/wireShader.geom")) {
        std::cerr << "Failed to build wireframe shader program." << std::endl;
        return -1;
    }

    // ------------------------------ Shadow Mapping Setup -------------------------------
    if(!shadowProgram.BuildFiles("shaders/tessShader.vert", "shaders/shadowShader.frag", nullptr, "shaders/tessShader.tesc", "shaders/tessShader.tese")) {
        std::cerr << "Failed to build shadow shader program." << std::endl;
        return -1;
    }

    // ------------------------------ Tessellation Setup ---------------------------------
    if(!tessProgram.BuildFiles("shaders/tessShader.vert", "shaders/planeShader.frag", nullptr, "shaders/tessShader.tesc", "shaders/tessShader.tese")) {
        std::cerr << "Failed to build tessellation shader program." << std::endl;
        return -1;
    }

    if(!tessWireProgram.BuildFiles("shaders/tessShader.vert", "shaders/wireShader.frag", "shaders/wireShader.geom", "shaders/tessShader.tesc", "shaders/tessShader.tese")) {
        std::cerr << "Failed to build tessellation wireframe shader program." << std::endl;
        return -1;
    }

    // ------------------------ OpenGL State and Input Setup -----------------------------
    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Register display callback function
    glutDisplayFunc(myDisplay);

    // Register keyboard callback function
    glutKeyboardFunc(myKeyboard);
    glutSpecialFunc(mySpecialKeys); // For special keys like F6

    //Register mouse callback function
    glutMouseFunc(myMouse);
    glutMotionFunc(myMouseMotion);


    // Register reshape callback function
    glutReshapeFunc(myReshape);


    glutMainLoop(); // Start the GLUT main loop

    return 0;
}