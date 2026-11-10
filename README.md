# Interactive Computer Graphics projects

My implementations of the eight projects of **CS 6610 Interactive Computer Graphics**, taught by [Cem Yuksel](http://www.cemyuksel.com/) at the University of Utah (Spring 2026). The course page, with the lectures and the original assignments, is at https://graphics.cs.utah.edu/courses/cs6610/spring2026/.

Each project folder contains the assignment text (`instructions.md`). The projects also implement the additional requirements for CS 6610 students.

## Projects

| # | Project | What it shows |
|---|---|---|
| 1 | Hello World | A FreeGLUT window whose background color changes over time. |
| 2 | Transformations | The vertices of an OBJ model drawn as points through a model-view-projection matrix, centered with its bounding box. |
| 3 | Shading | Triangles shaded with the Blinn model (ambient, diffuse and specular), with a light that rotates around the object and is drawn in the scene. |
| 4 | Textures | Materials read from the MTL file, with diffuse, specular and ambient textures and multiple materials per model. |
| 5 | Render Buffers | The object rendered into a texture and shown on a plane, with bilinear magnification and mipmapped, anisotropic minification. |
| 6 | Environment Mapping | A cube-map background, reflective objects with Blinn shading, and a reflective plane that shows the object through render-to-texture. |
| 7 | Shadow Mapping | Shadows from a spot light onto the plane and onto the object itself, with the light moved independently of the camera and drawn as a model. |
| 8 | Tessellation | Normal mapping, a geometry shader that shows the triangulation, displacement mapping with tessellation shaders, and shadows of the displaced surface. |

## Building

The projects were developed on Windows with MSYS2 (UCRT64), g++ (C++17), FreeGLUT and GLEW. Projects 1 to 7 run on OpenGL 3.3, and Project 8 needs OpenGL 4.1 for its tessellation shaders. [cyCodeBase](https://github.com/cemyuksel/cyCodeBase) and [LodePNG](https://lodev.org/lodepng/) are included in this repository.

1. Install the compiler and the libraries from an MSYS2 UCRT64 shell.

2. Build LodePNG once as a static library. The Makefile expects MSYS2 in `C:/msys64`.
   ```
   cd lodepng
   mingw32-make
   ```
3. Build a project from its own folder. The same command works for every project:
   ```
   cd Project4
   g++ -std=c++17 -isystem ../cyCodeBase -isystem ../lodepng main.cpp -o main.exe -L../lodepng -llodepng -lfreeglut -lglew32 -lopengl32 -lglu32
   ```

## Running

Run each program from its own folder, because the shaders and textures are loaded with relative paths.

| Project | Command |
|---|---|
| 1 | `main.exe` |
| 2 to 7 | `main.exe teapot.obj` |
| 4 | `main.exe yoda/yoda.obj` (a model with several materials) |
| 6 | `main.exe sphere.obj` (useful to check the reflections) |
| 8 | `main.exe teapot_normal.png teapot_disp.png` (without the second image, there is no displacement) |


## Credits

- Course, assignments and cyCodeBase (MIT license): Cem Yuksel, University of Utah, [CS 6610 Spring 2026](https://graphics.cs.utah.edu/courses/cs6610/spring2026/).
- LodePNG: Lode Vandevenne (zlib license, see `lodepng/LICENSE`).
- Cube map in Project 6: Emil Persson (Humus), CC BY 3.0 (see `Project6/cubemap/cubemap_readme.txt`).
- Yoda model in Project 4: Kecskeméti Zoltán "Ste-One" (see `Project4/yoda/yoda-credits.txt`).
- The other models and textures come with the course assignments.
