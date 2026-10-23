#include <GL/glew.h>
#include <GL/freeglut.h>

#include <cyMatrix.h>
#include <cyTriMesh.h>
#include <cyGL.h>

#include <iostream>

#define FAR_PLANE 100.0f
#define NEAR_PLANE 0.1f
#define FOV 45.0f


// Global variables
cy::TriMesh mesh;
cy::GLSLProgram program;
GLuint vao;
cy::Matrix4f mvpMatrix;
cy::Vec3f boundMin, boundMax;

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

void myDisplay() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Rebuild the model-view-projection matrix based on camera angles and distance
    cy::Vec3f center = (boundMax + boundMin) * 0.5f;
    cy::Matrix4f model = cy::Matrix4f::Scale(0.05f) * cy::Matrix4f::Translation(-center); // Center the mesh

    cy::Matrix4f view = cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, -cameraDistance)) *
                        cy::Matrix4f::RotationY(cameraAngleY) *
                        cy::Matrix4f::RotationX(cameraAngleX);

    cy::Matrix4f projection;
    if (isPerspective) {
        projection = cy::Matrix4f::Perspective(FOV * cy::Pi<float>() / 180.0f, aspectRatio, NEAR_PLANE, FAR_PLANE);
    } else {
        float sy = 1.0f / (cameraDistance * tan(FOV * cy::Pi<float>() /180 /2));
        float sx = sy / aspectRatio;
        float sz = -2.0f / (FAR_PLANE - NEAR_PLANE);
        float zOffset = -(FAR_PLANE + NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE);
        projection = cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, zOffset)) * cy::Matrix4f::Scale(sx, sy, sz);
    }

    mvpMatrix = projection * view * model;

    // Bind the shader program
    program.Bind();

    program["mvp"] = mvpMatrix;

    glBindVertexArray(vao);

    // Draw the mesh
    glDrawArrays(GL_POINTS, 0, mesh.NV());

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
    }
}

void mySpecialKeys(int key, int x, int y) {
    switch (key) {    
        case GLUT_KEY_F6: // F6 key
            // Reload the shaders
            if (!program.BuildFiles("shader.vert", "shader.frag"))
                std::cerr << "Failed to build shader program." << std::endl;
            else
                std::cout << "Shaders reloaded." << std::endl;
            break;
    }

    
    glutPostRedisplay(); // Request a redraw
}

void myMouse(int button, int state, int x, int y) {
    // Handle mouse input here
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        isLeftMouseButtonDown = true;
        leftMouseStartDrag = cy::Vec2f(x, y);
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        isLeftMouseButtonDown = false;
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

        cameraAngleX += delta.y * 0.01f;    //Movement along the y-axis affects the x-angle (pitch)
        cameraAngleY += delta.x * 0.01f;    //Movement along the x-axis affects the y-angle (yaw)

        leftMouseStartDrag = currentMousePos; // Update the last mouse position
    }
    if (isRightMouseButtonDown) {
        // Update camera position based on mouse movement
        cy::Vec2f currentMousePos(x, y);
        cy::Vec2f delta = currentMousePos - rightMouseStartDrag;

        if (cameraDistance > FAR_PLANE) {
            cameraDistance = FAR_PLANE;
        } else if (cameraDistance < NEAR_PLANE) {
            cameraDistance = NEAR_PLANE;
        } else {
            cameraDistance += delta.y * 0.01f; //Movement along the y-axis affects the distance
        }

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
    glutCreateWindow("Project2 OpenGL Window");

    // Initialize GLEW
    GLenum res = glewInit();
    if (res != GLEW_OK) {
        std::cerr << "Error: '" << glewGetErrorString(res) << "'" << std::endl;
        return 1;
    }

    // Load the mesh
    if (!mesh.LoadFromFileObj(argv[1])) {
        std::cerr << "Failed to load mesh." << std::endl;
        return -1;
    }
    mesh.ComputeBoundingBox();
    // Get the bounding box of the mesh
    boundMin = mesh.GetBoundMin();
    boundMax = mesh.GetBoundMax();
    
    // Set up the vertex array object and vertex buffer
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Upload vertex data to the GPU
    glBufferData(GL_ARRAY_BUFFER, mesh.NV() * sizeof(cy::Vec3f), &mesh.V(0), GL_STATIC_DRAW);
    
    // Set up vertex attribute pointers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);


    //Build and compile the shader program
    if (!program.BuildFiles("shader.vert", "shader.frag")) {
        std::cerr << "Failed to build shader program." << std::endl;
        return -1;
    }
    program.Bind();

    //Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);


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

    // Main loop
    glutMainLoop();

    return 0;
}