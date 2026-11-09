#version 410 core

layout(location = 0) in vec3 position;

//This tessellation shader only passes the vertex position to the next stage of the pipeline
//In this way the tessellation control shader can use the vertex position to compute the tessellation levels
void main()
{
    gl_Position = vec4(position, 1.0);
}