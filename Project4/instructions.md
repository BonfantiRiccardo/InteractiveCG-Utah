# Project 4 - Textures
In this project we will add color variations using textures.

## Requirements

- Obtain the diffuse texture image filename from the mtl file associated with the obj file.
- You can use the [cyTriMesh code release](http://www.cemyuksel.com/cyCodeBase/code.html#cyTriMesh) for parsing mtl files, which is done while parsing obj files. Make sure to use the latest version of this code release.
- You can use the files in this teapot.zip file for your tests.
- You can use the [LodePNG](http://lodev.org/lodepng/) library for decoding png image files.
- Your program must take the name of the .obj file as its first command-line argument.
- Generate a vertex buffer for texture coordinates.
- The diffuse texture should be correctly displayed on the object.


## Additional Requirements for CS 6610 Students
- Include the specular texture, specified in the mtl file, for adjusting the specular color of the object.

### Optional

- Provide support for objects with multiple materials, some of which can have textures.
- Set all material properties using the values in the mtl file.
You can use the files in this yoda.zip file for your tests.

## Notes

- Note that in 'teapot.zip' the vertex positions, vertex normals, and texture coordinates are not aligned. Thus if you want to use drawElements, you need to preprocess the mesh to generate correct the correctly ordered attributes with the new element indices. This can be done by iterating through all faces and tracking unique attributes in a set. An easier approach would be to revert to drawArrays.