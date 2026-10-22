#include <GL/glew.h>
#include <GL/freeglut.h>

#include <cyMatrix.h>
#include <cyTriMesh.h>
#include <cyGL.h>

#include <iostream>

//Camera control variables
#define FAR_PLANE 100.0f
#define NEAR_PLANE 0.1f
#define FOV 45.0f

//Material Properties
#define K_A cy::Vec3f(0.1f, 0.0f, 0.0f)     // Dark red
#define K_D cy::Vec3f(1.0f, 0.0f, 0.0f)     // Red
#define K_S cy::Vec3f(1.0f, 1.0f, 1.0f)     // White specular highlight
#define SHININESS 32.0f


// Global variables
cy::TriMesh mesh;
cy::GLSLProgram program, lightProgram;
GLuint vao, lightVAO;
cy::Matrix4f mvpMatrix;
cy::Matrix4f modelView;
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

// Light Properties
float lightAngleX = 0.0f;
float lightAngleY = 0.0f;
float lightDistance = 2.0f;
bool isDraggingLight = false; // Flag to indicate if the light is being dragged


void myDisplay() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //Rebuild the MVP matrix based on the current camera parameters
    cy::Vec3f center = (boundMin + boundMax) * 0.5f;
    cy::Matrix4f model = cy::Matrix4f::Scale(0.05f) * cy::Matrix4f::Translation(-center);

    cy::Matrix4f view = cy::Matrix4f::Translation(cy::Vec3f(0.0f, 0.0f, -cameraDistance)) * cy::Matrix4f::RotationX(cameraAngleX) * cy::Matrix4f::RotationY(cameraAngleY);

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

    modelView = view * model;
    mvpMatrix = projection * view * model;

    cy::Vec3f lightWorld = (cy::Matrix4f::RotationY(lightAngleY) * cy::Matrix4f::RotationX(lightAngleX) * cy::Vec3f(0.0f, 0.0f, lightDistance)).XYZ();
    cy::Vec3f lightEye = (view * cy::Vec4f(lightWorld, 1.0f)).XYZ();

    program.Bind();
    program["mvp"] = mvpMatrix;
    program["mvNormal"] = modelView.GetSubMatrix3().GetInverse().GetTranspose();
    program["modelView"] = modelView;
    program["K_A"] = K_A;
    program["K_D"] = K_D;
    program["K_S"] = K_S;
    program["shininess"] = SHININESS;
    program["lightPos"] = lightEye;
    program["isPerspective"] = isPerspective ? 1 : 0;
    glBindVertexArray(vao);

    // Draw the mesh by using the indices stored in the mesh (3 times the number of faces since each face has 3 vertices)
    glDrawElements(GL_TRIANGLES, mesh.NF() * 3, GL_UNSIGNED_INT, 0);

    // Draw the light source as a small point
    lightProgram.Bind();
    lightProgram["mvp"] = projection * view * cy::Matrix4f::Translation(lightWorld);

    glBindVertexArray(lightVAO);
    glDrawArrays(GL_POINTS, 0, 1); // Draw a single point for the light source

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
            if (!program.BuildFiles("shader.vert", "shader.frag") || 
                !lightProgram.BuildFiles("lightShader.vert", "lightShader.frag"))
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
    glutCreateWindow("Project3 OpenGL Window");

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
    //Compute the normals
    mesh.ComputeNormals();

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


    // Upload indices data to the GPU
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.NF() * sizeof(cy::TriMesh::TriFace), &mesh.F(0), GL_STATIC_DRAW);

    //Upload normals data to the GPU
    GLuint nbo;
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.NVN() * sizeof(cy::Vec3f), &mesh.VN(0), GL_STATIC_DRAW);

    // Set up normal attribute pointers
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // Build and compile the shader program
    if (!program.BuildFiles("shader.vert", "shader.frag")) {
        std::cerr << "Failed to build shader program." << std::endl;
        return -1;
    }
    program.Bind();


    // Generate light VAO and VBO
    GLuint lightVBO;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);

    glGenBuffers(1, &lightVBO);
    glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
    cy::Vec3f lightpoint = cy::Vec3f(0, 0, 0); // Light source position
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f), &lightpoint, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glPointSize(10.0f); // Set the point size for the light source

    // Build and compile the light shader program
    if(!lightProgram.BuildFiles("lightShader.vert", "lightShader.frag")) {
        std::cerr << "Failed to build light shader program." << std::endl;
        return -1;
    }
    lightProgram.Bind();

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