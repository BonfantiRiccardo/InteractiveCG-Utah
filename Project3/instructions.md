# Project 3 - Shading
In this project we will introduce simple lighting and shading.

## Requirements

The requirements of this project are separated into three steps. You can verify your implementation at the end of each step.

### Step 1: Display triangles
- Generate buffers for the triangle vertices of an .obj file.
- Draw the triangles using either glDrawArrays or glDrawElements.

At this point the object should appear flat, but you should not see any unusual empty spaces between triangles.

### Step 2: Normal buffer
- For shading the triangles we will need surface normals. Generate a normal buffer.
- The vertex shader should transform the normals using inverse transpose of the camera transformation matrix (exlucing projection).

It would be a good idea to color the triangles using the surface normal information, so that you can make sure that the normals are used and that they are transformed correctly.

Notice that the part of the surface facing up (y-direction) is green and the part facing right (x-direction) is red. The rest of the surface appears blue. Note that negative color values are clamped to [0,1].

### Step 3: Blinn shading
- Implement Blinn shading in the fragment shader, including ambient, diffuse, and specular components.
- The material properties can be hard-coded as constants or uniform parameters.
- The light position can be hard-coded as a contant or a uniform parameter.

It would be a good idea to implement each component (ambient, diffuse, and specular) separately and see if you are getting expected results. Then, you can combine all components.
ambient + diffuse specular ambient + diffuse + specular


## Additional Requirements for CS 6610 Students
Rotate the light around the object center using two angles.
When the CTRL key is down, left mouse click and drag should adjust the light rotation around the object.

### Optional
Display the light as a separate object.