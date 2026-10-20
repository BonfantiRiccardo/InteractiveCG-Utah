# Project 2 - Transformations

In this project we will display the vertices of an object. This will involve generating a vertex buffer, computing the camera transformations, and very simple GLSL shaders.

## Requirements

The requirements of this project are separated into three steps. You might be able to verify your implementation at the end of each step, but there is no guarantee that you will have a working implementation before you complete all three steps.

### Step 1: Vertex buffers
- Open and read the vertex data from an .obj file.
You can use this teapot.obj file for your tests.
You can use the [cyTriMesh](http://www.cemyuksel.com/cyCodeBase/code.html#cyTriMesh) code release for parsing the obj file.
- Your program must take the name of the .obj file as its first command-line argument.
- Generate and bind a vertex array object.
- You can use the [GLEW](http://glew.sourceforge.net/) library for initializing OpenGL extension functions. Do not forget to call glewInit after you create the OpenGL window.
- Generate a vertex buffer and set its data using the vertices you read from the .obj file.
- You can refer to [this tutorial](http://ogldev.atspace.co.uk/www/tutorial02/tutorial02.html) regarding the steps for generating the vertex buffer. Note that this tutorial does not include a vertex array object, but your implementation should. You can find a detailed description of vertex array objects and how to use them [here](https://yaakuro.gitbook.io/opengl-4-5/).
- Draw the contents of the buffer in your display function.
- At the end of this step you should see a single (white) point at the center of the screen, but depending on the settings of your OpenGL environment, you may see a completely blank screen instead.

### Step 2: GLSL shaders
- Write a simple vertex shader and a simple fragment shader.
- The vertex shader should transform all vertices by multiplying them with a model-view-projection matrix.
- The fragment shader should simply return a constant color value.
- You can use any color value you like.
- Compile the shaders into a GLSL program and use it for your draw call.
- You can use the [cyGL](http://www.cemyuksel.com/cyCodeBase/code.html#cyGL) code release for handling the shaders.
- If your vertex shader simply uses the given vertex position (without multiplying with any matrix), you should see a single point at the center of the screen at the end of this step. If your vertex shader multiplies the given vertex positions by 0.05, you should see all vertices of the teapot, as shown below.


### Step 3: Transformations
- Set the camera transformation using two rotation angles and one translation that determines the distance of the object from the camera.
- You can use the [cyMatrix](http://www.cemyuksel.com/cyCodeBase/code.html#cyMatrix) code release for setting up your transformation matrices.
- The left mouse button (and drag) should adjust the camera angles.
- The right mouse button (and drag) should adjust the camera distance.
- Use perspective transformation for projection.
- Send the matrix to your vertex shader as a uniform parameter.


## Additional Requirements for CS 6610 Students
- Pressing the F6 key should recompile your GLSL shaders.
- Center the object (using its bounding box) by adjusting the transformation matrix.

### Optional
- Pressing the P key switches between perspective and orthogonal transformation.
- When using orthogonal transformation, use one over camera distance as a uniform scale factor.