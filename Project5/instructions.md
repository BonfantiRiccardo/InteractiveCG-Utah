# Project 5 - Render Buffers
In this project we will explore the render-to-texture features of OpenGL.

## Requirements
- Your program must take the name of the .obj file as its first command-line argument.
- The object should be rendered using the diffuse texture, if one is specified in the mtl file.
- Instead of rendering the object directly to the OpenGL viewport, the object should be renderered to a texture via a render buffer.
- You can use the [GLRenderTexture](http://www.cemyuksel.com/cyCodeBase/docs/classcy_1_1GLRenderTexture.html) class for generating and using a render buffer.
- Display the rendered texture by mapping it on a square-shaped plane.
- As before, left mouse button (and drag) should adjust the camera angles, and right mouse button (and drag) should adjust the camera distance, used for rendering the object to the texture.
- If the ALT/Option key is pressed, the left and right mouse buttons (and drag) should control the same view parameters for rendering the plane.
- Add some small constant to color of the plane to separate it from the background color.

## Additional Requirements for CS 6610 Students

- The rendered texture should use bilinear filtering for magnification and mip-mapping with anisotropic filtering for minification.