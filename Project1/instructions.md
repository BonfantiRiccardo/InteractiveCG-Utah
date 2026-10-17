# Project 1 - Hello World
This first project is about setting up the environment for using OpenGL. We will create an OpenGL window and color it using basic OpenGL functions.

## Requirements

- Use C/C++.
- Use GLUT, FreeGLUT, GLFW, or another UI library of your choosing to create an OpenGL window.
- The OpenGL viewport size must be explicitly specified.
- Set the background color of the window using OpenGL calls.
- The "Esc" key must close the window.
- All OpenGL-related headers must be placed under the GL directory of an include path and included using forward slash. If you use GLFW, it can be in its default header location.
```c++
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>
```

## Additional Requirements for CS 6610 Students

- Automatically animate the background color.


## Notes

If you are using GLUT or FreeGLUT, pay attention to GLUT functions like the following:

- glutInit
- glutInitDisplayMode
- glutInitWindowSize
- glutCreateWindow
- glutMainLoop
- glutLeaveMainLoop
- glutPostRedisplay
- glutSwapBuffers
- glutDisplayFunc
- glutKeyboardFunc
- glutIdleFunc