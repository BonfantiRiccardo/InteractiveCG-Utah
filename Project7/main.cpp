#include <GL/glew.h>
#include <GL/freeglut.h>

#include <cyMatrix.h>
#include <cyTriMesh.h>
#include <cyGL.h>

#include "lodepng.h"

#include <iostream>
#include <map>

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

struct Material {
    cyGLTexture2D *map_Ka; // Ambient texture map
    cyGLTexture2D *map_Kd; // Diffuse texture map
    cyGLTexture2D *map_Ks; // Specular texture map
    cy::Vec3f Ka, Kd, Ks; // Ambient, Diffuse, Specular colors

    float Ns; // Specular exponent
    int firstFaceIndex; // Index of the first face using this material
    unsigned int faceCount; // Number of faces using this material
};
std::vector<Material> materials; // Vector to hold materials for each mesh part

// Global variables
cy::TriMesh mesh, pointLightMesh;
cyGLTexture2D whiteTex; // White texture for parts without a diffuse texture
std::map<std::string, cyGLTexture2D> textureCache; // Map to store textures by their file paths
cy::GLSLProgram program, lightProgram, planeProgram, shadowProgram;
GLuint vao, lightVAO, planeVAO;
cy::Matrix4f mvpMatrix;
cy::Matrix4f modelView;
cy::Vec3f boundMin, boundMax;
float modelScale = 1.0f; // Scale factor to fit the mesh within the view frustum

cy::GLRenderDepth2D shadowMap; // Depth buffer for shadow mapping


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

int createWhiteTexture(cyGLTexture2D& texture) {
    const int width = 1;
    const int height = 1;
    unsigned char whitePixel[4] = { 255, 255, 255, 255 }; // RGBA for white

    // Initialize the texture with cyGL
    texture.Initialize();
    // Set the image for the texture
    texture.SetImage(whitePixel, 4, width, height);
    texture.SetFilteringMode(GL_NEAREST, GL_NEAREST); // No filtering needed for a single pixel

    return 0;
}

cyGLTexture2D* getTexture(const std::string& texturePath, const char* name) {
    // Check if the texture is already loaded
    if (name == nullptr || strlen(name) == 0) {
        return nullptr; // Return nullptr if no texture name is provided
    }
    std::string fullPath = texturePath + name;
    
    auto it = textureCache.find(fullPath);
    if (it != textureCache.end()) {
        return &it->second; // Return the cached texture
    }

    // If not, load the texture
    cyGLTexture2D newTexture;
    if (loadTexture(fullPath, newTexture) == 0) {
        textureCache[fullPath] = newTexture; // Store in cache
        return &textureCache[fullPath];
    } else {
        std::cerr << "Failed to load texture: " << name << " from path: " << texturePath << std::endl;
        textureCache.erase(fullPath); // Remove from cache if loading failed
        return nullptr;
    }
}

// ------------------------------ Rendering Functions ------------------------------
void myDisplay() {
    // ------------------------------ Update Camera and Light Matrices -----------------------------
    //Rebuild the MVP matrix based on the current camera parameters
    cy::Vec3f center = (boundMin + boundMax) * 0.5f;
    cy::Matrix4f model = cy::Matrix4f::Scale(modelScale) * cy::Matrix4f::Translation(-center);

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

    modelView = view * model;
    mvpMatrix = projection * view * model;

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
    shadowMap.Bind(); // Bind the depth buffer for off-screen rendering
    glClear(GL_DEPTH_BUFFER_BIT); // Clear the depth buffer

    glEnable(GL_POLYGON_OFFSET_FILL); // Enable polygon offset to avoid z-fighting
    glPolygonOffset(1.0f, 1.0f); // Set polygon offset parameters

    shadowProgram.Bind();
    cy::Matrix4f shadowMVP = lightProjection * lightView * model;
    shadowProgram["modelViewProjectionMatrix"] = shadowMVP;

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.NF() * 3); // Draw the entire mesh to the shadow map

    glDisable(GL_POLYGON_OFFSET_FILL); // Disable polygon offset after rendering to shadow map

    shadowMap.Unbind(); // Unbind the depth buffer after rendering

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the default framebuffer

    // ------------------------------ Draw main scene ------------------------------------
    program.Bind();
    program["mvp"] = mvpMatrix;
    program["mvNormal"] = modelView.GetSubMatrix3().GetInverse().GetTranspose();
    program["modelView"] = modelView;

    program["lightPos"] = lightEye;
    program["isPerspective"] = isPerspective ? 1 : 0;

    program["diffuseTexture"] = 0; // Set the texture unit to 0
    program["specularTexture"] = 1; // Set the texture unit to 1
    program["ambientTexture"] = 2; // Set the texture unit to 2

    shadowMap.BindTexture(3); // Bind the depth texture to texture unit 3
    program["shadowMap"] = 3; // Set the texture unit to 3 for the shadow map
    program["spotLightDir"] = spotDir; // Set the spotlight direction in eye space
    program["spotLightCutoff"] = spotCutoff; // Set the spotlight cutoff angle
    program["shadowMatrix"] = shadowBias * lightProjection * lightView * model; // Set the shadow matrix for transforming to light space

    for (const Material& mat : materials) {
        mat.map_Kd->Bind(0); // Bind diffuse texture to texture unit 0
        mat.map_Ks->Bind(1); // Bind specular texture to texture unit 1
        mat.map_Ka->Bind(2); // Bind ambient texture to texture unit 2

        program["K_A"] = mat.Ka;
        program["K_D"] = mat.Kd;
        program["K_S"] = mat.Ks;
        program["shininess"] = mat.Ns;

        // Draw the mesh part corresponding to this material
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, mat.firstFaceIndex * 3, mat.faceCount * 3);
    }

    //------------------------------ Draw the light source ------------------------------------
    // Draw the light source as a small point
    lightProgram.Bind();
    // Scale down the light source mesh and position it at the light's world position
    lightProgram["mvp"] = projection * view * lightFrame * cy::Matrix4f::Scale(0.1f);

    glBindVertexArray(lightVAO);
    glDrawArrays(GL_TRIANGLES, 0, pointLightMesh.NF() * 3); // Draw the light source mesh

    // ------------------------------ Draw the plane ------------------------------------
    
    // Draw the plane
    planeProgram.Bind();
    planeProgram["mvp"] = planeMVP;
    planeProgram["mvNormal"] = view.GetSubMatrix3().GetInverse().GetTranspose();
    planeProgram["modelView"] = view;
    planeProgram["lightPos"] = lightEye;
    planeProgram["isPerspective"] = isPerspective ? 1 : 0;
    planeProgram["shadowMatrix"] = shadowBias * lightProjection * lightView;
    planeProgram["K_A"] = K_A;
    planeProgram["K_D"] = K_D;
    planeProgram["K_S"] = K_S;
    planeProgram["shininess"] = SHININESS;

    planeProgram["shadowMap"] = 3; // Set the shader uniform to use texture unit 3

    planeProgram["spotLightDir"] = spotDir; // Set the spotlight direction in eye space
    planeProgram["spotLightCutoff"] = spotCutoff; // Set the spotlight cutoff angle

    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4); // Draw the plane


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
            if (!program.BuildFiles("shaders/shader.vert", "shaders/shader.frag") || 
                !lightProgram.BuildFiles("shaders/lightShader.vert", "shaders/lightShader.frag") ||
                !planeProgram.BuildFiles("shaders/planeShader.vert", "shaders/planeShader.frag") ||
                !shadowProgram.BuildFiles("shaders/shadowShader.vert", "shaders/shadowShader.frag"))
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
        std::cerr << "Usage: " << argv[0] << " <path_to_obj_file>" << std::endl;
        return -1;
    }

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Project7 OpenGL Window");

    // Initialize GLEW
    GLenum res = glewInit();
    if (res != GLEW_OK) {
        std::cerr << "Error: '" << glewGetErrorString(res) << "'" << std::endl;
        return 1;
    }

    //------------------------------ Main mesh setup ---------------------------------
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

    //Compute the normals only when mesh does not have them already
    if(!mesh.HasNormals()) mesh.ComputeNormals();

    //Compute vertices, normals and texture vectors
    std::vector<cy::Vec3f> vert, norm;
    std::vector<cy::Vec2f> tex;
    for (unsigned int i = 0; i < mesh.NF(); i++) {
        vert.push_back(mesh.V(mesh.F(i).v[0]));
        vert.push_back(mesh.V(mesh.F(i).v[1]));
        vert.push_back(mesh.V(mesh.F(i).v[2]));

        norm.push_back(mesh.VN(mesh.FN(i).v[0]));
        norm.push_back(mesh.VN(mesh.FN(i).v[1]));
        norm.push_back(mesh.VN(mesh.FN(i).v[2]));

        tex.push_back(mesh.VT(mesh.FT(i).v[0]).XY());
        tex.push_back(mesh.VT(mesh.FT(i).v[1]).XY());
        tex.push_back(mesh.VT(mesh.FT(i).v[2]).XY());
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

    //Upload texture coordinates data to the GPU
    GLuint tbo;
    glGenBuffers(1, &tbo);
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, tex.size() * sizeof(cy::Vec2f), tex.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

    //Upload texture data to the GPU
    //Build the full path to the texture file based on the obj file's directory
    std::string objFilePath = argv[1];
    // Extract the directory from the obj file path
    std::string directory = objFilePath.substr(0, objFilePath.find_last_of("/\\") + 1);

    createWhiteTexture(whiteTex); // Create a white texture for parts without a diffuse texture

    if (mesh.NM() == 0)
        materials.push_back(Material{ &whiteTex, &whiteTex, &whiteTex, K_A, K_D, K_S, SHININESS, 0, mesh.NF() });

    else {
        for (unsigned int i = 0; i < mesh.NM(); i++) {
            cy::TriMesh::Mtl& mtl = mesh.M(i);
            Material mat;
            cyGLTexture2D* ambientTex = getTexture(directory, mtl.map_Ka.data);
            cyGLTexture2D* diffuseTex = getTexture(directory, mtl.map_Kd.data);
            cyGLTexture2D* specularTex = getTexture(directory, mtl.map_Ks.data);

            mat.map_Ka = ambientTex ? ambientTex : &whiteTex;
            mat.map_Kd = diffuseTex ? diffuseTex : &whiteTex;
            mat.map_Ks = specularTex ? specularTex : &whiteTex;
            mat.Ka = ambientTex ?  K_A : cy::Vec3f(mtl.Ka[0], mtl.Ka[1], mtl.Ka[2]); // Set Ka to white to use the ambient texture color
            mat.Kd = diffuseTex ? K_D : cy::Vec3f(mtl.Kd[0], mtl.Kd[1], mtl.Kd[2]); // Set Kd to white to use the diffuse texture color
            mat.Ks = specularTex ? K_S : cy::Vec3f(mtl.Ks[0], mtl.Ks[1], mtl.Ks[2]); // Set Ks to white to use the specular texture color
            mat.Ns = (mtl.Ns > 0.0f) ? mtl.Ns : SHININESS; // Use the material's shininess if available, otherwise use default
            mat.firstFaceIndex = mesh.GetMaterialFirstFace(i);
            mat.faceCount = mesh.GetMaterialFaceCount(i);

            materials.push_back(mat);
        }
    }

    //---------- Depth Buffer Setup ---------
    shadowMap.Initialize(true, 1024, 1024); // Initialize the shadow map with desired dimensions and formats
    shadowMap.SetTextureFilteringMode(GL_LINEAR, GL_LINEAR); // Set texture filtering mode for the shadow map

    // Build and compile the shader program
    if (!program.BuildFiles("shaders/shader.vert", "shaders/shader.frag")) {
        std::cerr << "Failed to build shader program." << std::endl;
        return -1;
    }
    program.Bind();


    // ------------------------------ Plane Setup ------------------------------------
    //Create vertices for the plane
    float planeZ = (boundMin.z - boundMax.z) * 0.5f * modelScale; // Position the plane slightly below the mesh

    std::vector<cy::Vec3f> planeVertices = {
        cy::Vec3f(-5.0f, -5.0f, planeZ), // Bottom-left
        cy::Vec3f(5.0f, -5.0f, planeZ),  // Bottom-right
        cy::Vec3f(5.0f, 5.0f, planeZ),   // Top-right
        cy::Vec3f(-5.0f, 5.0f, planeZ)   // Top-left
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

    GLuint planeNBO;
    std::vector<cy::Vec3f> planeNormals (4, cy::Vec3f(0.0f, 0.0f, 1.0f)); // Normals pointing up
    
    glGenBuffers(1, &planeNBO);
    glBindBuffer(GL_ARRAY_BUFFER, planeNBO);
    glBufferData(GL_ARRAY_BUFFER, planeVertices.size() * sizeof(cy::Vec3f), planeNormals.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

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

    //------------------------------ Shadow program setup --------------------------------
    if (!shadowProgram.BuildFiles("shaders/shadowShader.vert", "shaders/shadowShader.frag")) {
        std::cerr << "Failed to build shadow shader program." << std::endl;
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