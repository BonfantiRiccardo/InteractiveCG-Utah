#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>


void myDisplay() {
    //Clear the color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //Swap buffers to display the rendered image
    glutSwapBuffers();
}

void myKeyboard(unsigned char key, int x, int y) {
    //Handle keyboard input
    switch (key) {
        case 27: // Escape key
            exit(0);
            break;
        default:
            break;
    }
}

void myIdle() {
    //Get elapsed time in milliseconds since the program started
    int currentTime = glutGet(GLUT_ELAPSED_TIME);

    float speedR = 1; // Speed of red color change
    float speedG = 0.5f; // Speed of green color change
    float speedB = 0.25f; // Speed of blue color change

    float r=0, g=0, b=0; // Initialize color values
    float timeInSeconds = currentTime / 1000.0f; // Convert milliseconds to seconds

    // Update the color values based on the elapsed time and speed
    r = 0.5 + 0.5 * sin(timeInSeconds * speedR);
    g = 0.5 + 0.5 * sin(timeInSeconds * speedG);
    b = 0.5 + 0.5 * sin(timeInSeconds * speedB);


    //Set the clear color before the clear call so the background color is updated correctly
    glClearColor(r,g,b, 1.0f);

    //Request a redraw of the window
    glutPostRedisplay();
}

void myReshape(int width, int height) {
    //Set the viewport to cover the new window size
    glViewport(0, 0, width, height);
}

int main (int argc, char** argv) {

    //Intialize GLUT
    glutInit(&argc, argv);

    //Create a window with the specified title
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutCreateWindow("Project1 OpenGL Window");

    //Register display callback function
    glutDisplayFunc(myDisplay);

    //Register keyboard callback function
    glutKeyboardFunc(myKeyboard);

    // Register idle callback function to continuously update the display
    glutIdleFunc(myIdle);

    // Register window reshape callback
    glutReshapeFunc(myReshape);

    //Call main loop
    glutMainLoop();

    return 0;
}