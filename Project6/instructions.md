# Project 6 - Environment Mapping
In this project we will implement simple environment mapping. This will include both the background and the reflections on objects.

## Requirements

- Your program must take the name of the .obj file as its first command-line argument.
- The files for the cube-map images forming the background and the reflection environment can be hard-coded.
- You can use this cube-map image set for your tests. It is recommended that you use the y-up convention for this project, since this cube-map assumes y-up.

### Step 1: Display the background
- Use one of the following background object types to display the background:
- - Option 1: One or two triangles that cover the entire camera view.
- - Option 2: A cube centered at the camera position. You can use this cube.obj file for the cube or hard-code its vertex positions.
- Writing to the depth buffer must be disabled while drawing the background object.
- Map the cube-map enviroment onto the background object, such that rotating the camera properly rotates the texture on the cubemap, producing an illusion of an environment.

### Step 2: Reflections on a sphere
- Spheres are useful for debuging reflections. You can use this sphere.obj file for your tests.
- The sphere must display reflections of the environment texture.
- The reflections must match the orientation of the camera.

### Step 3: Reflections on objects
- The final reflections must be combined with Blinn shading.
- The reflections should be properly displaed on the given obj file. You can use this teapot.obj file for your tests, which includes a teapot model oriented such that y-axis is aligned with the up direction.



## Additional Requirements for CS 6610 Students
- Draw a plane with environment reflections underneath the object.
- Use render-to-texture operations to produce the reflection of the teapot on the plane.