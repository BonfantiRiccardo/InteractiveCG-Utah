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
#define K_D cy::Vec3f(.5f, .5f, .5f)     // White
#define K_S cy::Vec3f(1.0f, 1.0f, 1.0f)     // White specular highlight
#define SHININESS 128.0f


// Global variables
cy::TriMesh mesh;
cy::GLSLProgram program, bgProgram, planeProgram;
GLuint vao, bgVAO, planeVAO;
cy::Vec3f boundMin, boundMax;
float modelScale = 1.0f; // Scale factor to fit the mesh within the view frustum

cy::GLTextureCubeMap envMap;
cy::GLRenderTexture2D reflectionBuffer;
float planeHeight = 0.0f; // Height of the reflective plane

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

// Light Properties
float lightAngleX = 0.0f;
float lightAngleY = 0.0f;
float lightDistance = 2.0f;
bool isDraggingLight = false; // Flag to indicate if the light is being dragged

int windowWidth = 800;
int windowHeight = 600;


// ------------------------------ Rendering Functions ------------------------------
void myDisplay() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the default framebuffer

    //Rebuild the MVP matrix based on the current camera parameters
    cy::Vec3f center = (boundMin + boundMax) * 0.5f;
    cy::Matrix4f model = cy::Matrix4f::Scale(modelScale) * cy::Matrix4f::Translation(-center);

    cy::Matrix4f view = cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, -cameraDistance)) * cy::Matrix4f::RotationX(cameraAngleX) * cy::Matrix4f::RotationY(cameraAngleY);

    cy::Matrix4f projection = cy::Matrix4f::Perspective(FOV * cy::Pi<float>() / 180.0f, aspectRatio, NEAR_PLANE, FAR_PLANE);

    cy::Vec3f lightWorld = (cy::Matrix4f::RotationY(lightAngleY) * cy::Matrix4f::RotationX(lightAngleX) * cy::Vec3f(0.0f, 0.0f, lightDistance)).XYZ();

    cy::Matrix4f mirror = cy::Matrix4f::Translation(cy::Vec3f(0, planeHeight, 0)) * cy::Matrix4f::Scale(1.0f, -1.0f, 1.0f) * cy::Matrix4f::Translation(cy::Vec3f(0, -planeHeight, 0));

    // Lambda function to draw the teapot with a given model-view matrix
    auto drawTeapot = [&](cy::Matrix4f mat) {
        // Render the mesh with reflections
        program.Bind();
        cy::Matrix4f mv = mat * model;
        program["mvp"] = projection * mv;
        program["mvNormal"] = mv.GetSubMatrix3().GetInverse().GetTranspose();
        program["modelView"] = mv;

        program["lightPos"] = (mat * cy::Vec4f(lightWorld, 1.0f)).XYZ();
        program["viewToWorld"] = mat.GetSubMatrix3().GetTranspose();
        program["envMap"] = 3; // Set the texture unit to 3 for the cubemap

        program["K_A"] = K_A;
        program["K_D"] = K_D;
        program["K_S"] = K_S;
        program["shininess"] = SHININESS;

        envMap.Bind(3); // Bind the cubemap to texture unit 3

        // Draw the mesh
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh.NF() * 3);
    };
    
    //Mirrored teapot rendering
    reflectionBuffer.Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the reflection buffer
    drawTeapot(view * mirror); // Draw the teapot into the reflection buffer
    reflectionBuffer.Unbind();


    //BG cubemap rendering
    bgProgram["inverseViewProjectionMatrix"] = (projection * cy::Matrix4f(view.GetSubMatrix3())).GetInverse();
    glDepthMask(GL_FALSE); // Disable depth writing for background
    bgProgram.Bind();
    bgProgram["envMap"] = 3; // Set the texture unit to 3

    glBindVertexArray(bgVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // Draw the background quad
    glDepthMask(GL_TRUE); // Re-enable depth writing for the rest of the scene

    drawTeapot(view); // Draw the teapot normally

    // Draw the reflective plane
    planeProgram.Bind();
    planeProgram["mvp"] = projection * view;
    planeProgram["viewToWorld"] = view.GetSubMatrix3().GetTranspose();
    planeProgram["envMap"] = 3; // Set the texture unit to 3
    planeProgram["modelView"] = view;
    planeProgram["mvNormal"] = view.GetSubMatrix3().GetInverse().GetTranspose();
    reflectionBuffer.BindTexture(0); // Bind the reflection buffer texture to texture unit 0
    planeProgram["reflectionTexture"] = 0; // Set the texture unit to 0
    planeProgram["screenSize"] = cy::Vec2f((float) windowWidth, (float) windowHeight);

    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // Draw the reflective plane as a quad

    glutSwapBuffers();
}

void myKeyboard(unsigned char key, int x, int y){
    switch (key) {
        case 27: // ESC key
            std::cout << "Exiting the program." << std::endl;
            exit(0);
            break;
    }
}

void mySpecialKeys(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_F6: // F6 key
            // Reload the shaders
            if (!program.BuildFiles("shaders/shader.vert", "shaders/shader.frag") ||
                !bgProgram.BuildFiles("shaders/bgShader.vert", "shaders/bgShader.frag") ||
                !planeProgram.BuildFiles("shaders/shader.vert", "shaders/planeShader.frag")) {
                std::cerr << "Failed to build shader program." << std::endl;
            } else
                std::cout << "Shaders reloaded." << std::endl;
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
    windowWidth = width;
    windowHeight = height;
    reflectionBuffer.Resize(4, windowWidth, windowHeight); // Resize the reflection buffer to match the new window size
}


// ------------------------------ Main Function ------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_obj_file>" << std::endl;
        return -1;
    }

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Project6 OpenGL Window");

    // Initialize GLEW
    GLenum res = glewInit();
    if (res != GLEW_OK) {
        std::cerr << "Error: '" << glewGetErrorString(res) << "'" << std::endl;
        return 1;
    }

    envMap.Initialize();
    for (int i = 0; i < 6; ++i) {
        std::string facenName;
        switch (i) {
            case 0: facenName = "posx"; break;
            case 1: facenName = "negx"; break;
            case 2: facenName = "posy"; break;
            case 3: facenName = "negy"; break;
            case 4: facenName = "posz"; break;
            case 5: facenName = "negz"; break;
        }

        std::vector<unsigned char> image; //the raw pixels
        unsigned int width, height;
        if(lodepng::decode(image, width, height, "cubemap/cubemap_" + facenName + ".png")) {
            std::cerr << "Failed to load cubemap face: " << facenName << std::endl;
            return -1;
        }

        envMap.SetImage(static_cast<cy::GLTextureCubeMap::Side>(i), GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, image.data(), width, height);
    }
    envMap.BuildMipmaps();
    envMap.SetFilteringMode(GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    cy::GLTextureCubeMap::SetSeamless(true); // Enable seamless cubemap sampling

    // Load the mesh
    if (!mesh.LoadFromFileObj(argv[1])) {
        std::cerr << "Failed to load mesh." << std::endl;
        return -1;
    }
    mesh.ComputeBoundingBox();
    // Get the bounding box of the mesh
    boundMin = mesh.GetBoundMin();
    boundMax = mesh.GetBoundMax();
    //Scale the mesh to fit within the view frustum
    float radius = (boundMax - boundMin).Length() * 0.5f;
    modelScale = 1.0f / radius;

    planeHeight = (boundMin.y - boundMax.y) * 0.5f * modelScale; // Set the plane height at the bottom of the mesh after scaling
    //Compute the normals only when mesh does not have them already
    if(!mesh.HasNormals()) mesh.ComputeNormals();

    //Compute vertices and normals vectors
    std::vector<cy::Vec3f> vert, norm;
    for (unsigned int i = 0; i < mesh.NF(); i++) {
        vert.push_back(mesh.V(mesh.F(i).v[0]));
        vert.push_back(mesh.V(mesh.F(i).v[1]));
        vert.push_back(mesh.V(mesh.F(i).v[2]));

        norm.push_back(mesh.VN(mesh.FN(i).v[0]));
        norm.push_back(mesh.VN(mesh.FN(i).v[1]));
        norm.push_back(mesh.VN(mesh.FN(i).v[2]));
    }

    // Set up the vertex array object and vertex buffer
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Upload vertex data to the GPU
    glBufferData(GL_ARRAY_BUFFER, vert.size() * sizeof(cy::Vec3f), vert.data(), GL_STATIC_DRAW);

    // Set up vertex attribute pointers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    //Upload normals data to the GPU
    GLuint nbo;
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, norm.size() * sizeof(cy::Vec3f), norm.data(), GL_STATIC_DRAW);

    // Set up normal attribute pointers
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // Build and compile the shader program
    if (!program.BuildFiles("shaders/shader.vert", "shaders/shader.frag")) {
        std::cerr << "Failed to build shader program." << std::endl;
        return -1;
    }
    program.Bind();

    // Full-screen quad in clip space for the background
    std::vector<cy::Vec3f> quadVertices = {
        cy::Vec3f(-1.0f, -1.0f, 0.0f),
        cy::Vec3f(1.0f, -1.0f, 0.0f),
        cy::Vec3f(1.0f, 1.0f, 0.0f),
        cy::Vec3f(-1.0f, 1.0f, 0.0f)
    };

    // Set up the vertex array object and vertex buffer for the background quad
    GLuint bgVBO;
    glGenVertexArrays(1, &bgVAO);
    glBindVertexArray(bgVAO);

    glGenBuffers(1, &bgVBO);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO);
    glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(cy::Vec3f), quadVertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    if (!bgProgram.BuildFiles("shaders/bgShader.vert", "shaders/bgShader.frag")) {
        std::cerr << "Failed to build background shader program." << std::endl;
        return -1;
    }

    // Set up the vertex array object and vertex buffer for the reflective plane
    std::vector<cy::Vec3f> planeVertices = {
        cy::Vec3f(-2.0f, planeHeight, -2.0f),
        cy::Vec3f(2.0f, planeHeight, -2.0f),
        cy::Vec3f(2.0f, planeHeight, 2.0f),
        cy::Vec3f(-2.0f, planeHeight, 2.0f)
    };

    std::vector<cy::Vec3f> planeNormals = {
        cy::Vec3f(0.0f, 1.0f, 0.0f),
        cy::Vec3f(0.0f, 1.0f, 0.0f),
        cy::Vec3f(0.0f, 1.0f, 0.0f),
        cy::Vec3f(0.0f, 1.0f, 0.0f)
    };

    glGenVertexArrays(1, &planeVAO);
    glBindVertexArray(planeVAO);

    GLuint planeVBO, planeNBO;
    glGenBuffers(1, &planeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, planeVertices.size() * sizeof(cy::Vec3f), planeVertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glGenBuffers(1, &planeNBO);
    glBindBuffer(GL_ARRAY_BUFFER, planeNBO);
    glBufferData(GL_ARRAY_BUFFER, planeNormals.size() * sizeof(cy::Vec3f), planeNormals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    reflectionBuffer.Initialize(true,  4, windowWidth, windowHeight);

    if (!planeProgram.BuildFiles("shaders/shader.vert", "shaders/planeShader.frag")) {
        std::cerr << "Failed to build plane shader program." << std::endl;
        return -1;
    }


    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

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
