# Project 7 - Shadow Mapping

In this project we will implement shadow mapping.

## Requirements

- Your program must take the name of the .obj file as its first command-line argument.
- Display the object and a plane underneath the object.
- Compute shadows from a spot light onto the plane and the object.
- The object must cast shadows onto itself, but the plane does not need to cast shadows.
- You can use the [GLRenderDepth](http://www.cemyuksel.com/cyCodeBase/docs/classcy_1_1GLRenderDepth.html) class for generating the depth map.

## Additional Requirements for CS 6610 Students

- The light position should be controlled independently of the camera position.
- Display the light position.

### Optional

- Display the light as an object. You can use the files in this light.zip file.
- Note that the light cannot properly illuminate the light object.

### Notes
There are numerous online tutorials on shadow mapping. Note that some of them are old and use old OpenGL functions. I would recommend the two tutorials below, though neither one uses hardware depth comparison:

- http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/
- https://learnopengl.com/#!Advanced-Lighting/Shadows/Shadow-Mapping

It would be a good idea to render the depth map and display the depth map on a plane, just to make sure that the depth map is correctly rendered (i.e. the object is visible in the depth map). Note that depending on the perspective projection matrix, all values in the depth map can be very similar, so the object may not be perceivable even if it is on the depth texture. It would be a good idea to use a debugger like GDebugger to see the contents of the rendered depth texture.

Also note that the GLRenderDepth2D class would require sampler2DShadow, when it is initialized as a depth comparison texture. If you would like to display the contents of this texture (for debuging purposes), do not initialize it as a depth comparison texture, so that you can use sampler2D instead.