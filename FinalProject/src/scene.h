#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <cyMatrix.h>
#include <cyTriMesh.h>

const float PI = 3.14159265f;
const int   MAX_LIGHTS = 8;
const int   PALETTE_SIZE = 6;
const cy::Vec3f PALETTE[PALETTE_SIZE] = { { 1, 1, 1 }, { 1, 0.75f, 0.5f }, { 0.55f, 0.7f, 1 },
                                          { 1, 0.35f, 0.3f }, { 0.4f, 1, 0.45f }, { 1, 0.62f, 0.62f } };

enum ObjectType { OBJECT_MESH, OBJECT_POINT_LIGHT, OBJECT_SPOT_LIGHT, OBJECT_DIRECTIONAL_LIGHT, OBJECT_TYPES };
const char* OBJECT_NAMES[OBJECT_TYPES] = { "teapot", "point light", "spot light", "directional light" };

// A mesh, or a light with the shape that marks it. A light shines along its local -z axis.
struct SceneObject {
    ObjectType   type;
    cy::Vec3f    position;
    cy::Matrix3f rotation;          // columns: the local axes in world space
    float        size;              // half of the largest side of the shape, in world units
    int          color = 0;         // index in PALETTE: albedo of a mesh, tint of a light
    float        intensity = 1;     // lights: irradiance at the distance reach, relative to the sky of the capture
    float        reach = 1;         // point and spot lights: distance at which the intensity applies
    float        cone = 25;         // spot lights: half angle in degrees
};

// Triangles of one object type inside the vertex buffer, scaled so that the largest side is 2.
struct Shape {
    int   first, count;             // vertices
    float base;                     // distance from the origin of the shape down to its lowest point
};

// Content of the light buffer, in the layout of the uniform block of shaders/lighting.glsl.
struct LightBlock {
    unsigned count;
    float    ambient, padding[2];
    float    lights[MAX_LIGHTS][12];    // position, direction, color: 4 floats each
};

std::vector<SceneObject> objects;
Shape shapes[OBJECT_TYPES];
int   selected = -1;                // index in objects, or -1
float ambient = 1;                  // scale of the light of the capture
bool  showLightShapes = true;
unsigned sceneVersion = 0;          // grows whenever an object changes, so cached light maps know when to rebuild

// Appends the triangles of an OBJ file as 8 floats per vertex (position and normal, each padded to 4).
// A centered shape gets its origin in the middle of its bounding box. Otherwise it keeps the origin of the file.
bool addObjShape(std::vector<float>& vertices, Shape& shape, const char* file, bool centered) {
    cy::TriMesh mesh;
    if (!mesh.LoadFromFileObj(file, false)) { std::cerr << "Cannot load " << file << std::endl; return false; }
    if (!mesh.HasNormals()) mesh.ComputeNormals();
    mesh.ComputeBoundingBox();
    cy::Vec3f low = mesh.GetBoundMin(), high = mesh.GetBoundMax();
    cy::Vec3f origin = centered ? (low + high) * 0.5f : cy::Vec3f(0, 0, 0);
    float scale = 2.0f / (high - low).Max();
    shape.first = int(vertices.size() / 8);
    shape.count = 3 * (int)mesh.NF();
    shape.base = (origin.z - low.z) * scale;
    for (int face = 0; face < (int)mesh.NF(); face++)
        for (int corner = 0; corner < 3; corner++) {
            cy::Vec3f p = (mesh.V(mesh.F(face).v[corner]) - origin) * scale, n = mesh.VN(mesh.FN(face).v[corner]);
            vertices.insert(vertices.end(), { p.x, p.y, p.z, 1, n.x, n.y, n.z, 0 });
        }
    return true;
}

// Appends a surface of revolution around the z axis, with one normal per face. The profile is a list of (radius, z).
void addRevolvedShape(std::vector<float>& vertices, Shape& shape, const std::vector<cy::Vec2f>& profile) {
    const int SIDES = 16;
    auto point = [&](int i, int side) {
        float angle = 2 * PI * side / SIDES;
        return cy::Vec3f(profile[i].x * std::cos(angle), profile[i].x * std::sin(angle), profile[i].y);
    };
    shape.first = int(vertices.size() / 8);
    shape.base = 1;
    for (int i = 0; i + 1 < (int)profile.size(); i++)
        for (int side = 0; side < SIDES; side++) {
            cy::Vec3f quad[4] = { point(i, side), point(i + 1, side), point(i + 1, side + 1), point(i, side + 1) };
            cy::Vec3f n = ((quad[2] - quad[0]) ^ (quad[3] - quad[1])).GetNormalized();
            for (int corner : { 0, 1, 2, 0, 2, 3 }) {
                cy::Vec3f p = quad[corner];
                vertices.insert(vertices.end(), { p.x, p.y, p.z, 1, n.x, n.y, n.z, 0 });
            }
        }
    shape.count = int(vertices.size() / 8) - shape.first;
}

// Builds the vertices of the four shapes: the teapot, a sphere, the pyramid of light.obj (apex at the origin, open toward -z)
// and an arrow that points along -z.
bool buildShapes(std::vector<float>& vertices) {
    std::vector<cy::Vec2f> sphere, arrow = { { 0, 1 }, { 0.07f, 1 }, { 0.07f, -0.3f }, { 0.25f, -0.3f }, { 0, -1 } };
    for (int i = 0; i <= 8; i++) sphere.push_back(cy::Vec2f(std::sin(PI * i / 8), std::cos(PI * i / 8)));
    if (!addObjShape(vertices, shapes[OBJECT_MESH], "../models/teapot/teapot.obj", true)) return false;
    addRevolvedShape(vertices, shapes[OBJECT_POINT_LIGHT], sphere);
    if (!addObjShape(vertices, shapes[OBJECT_SPOT_LIGHT], "../models/light/light.obj", false)) return false;
    addRevolvedShape(vertices, shapes[OBJECT_DIRECTIONAL_LIGHT], arrow);
    return true;
}

// True when two objects have the same type, transform, size, color and light settings.
bool sameObject(const SceneObject& a, const SceneObject& b) {
    return a.type == b.type && a.position == b.position && a.rotation == b.rotation && a.size == b.size &&
           a.color == b.color && a.intensity == b.intensity && a.reach == b.reach && a.cone == b.cone;
}

// Starts a new scene version if any object changed since the last call. Call it once per frame.
void trackSceneChanges() {
    static std::vector<SceneObject> last;
    if (last.size() == objects.size() && std::equal(objects.begin(), objects.end(), last.begin(), sameObject)) return;
    last = objects;
    sceneVersion++;
}

// Number of lights among the objects.
int lightCount() {
    int count = 0;
    for (const SceneObject& object : objects) count += object.type != OBJECT_MESH;
    return count;
}

// Packs the lights of the scene for the shaders.
LightBlock packLights() {
    LightBlock block = {};
    block.ambient = ambient;
    for (const SceneObject& object : objects) {
        if (object.type == OBJECT_MESH || block.count == MAX_LIGHTS) continue;
        bool directional = object.type == OBJECT_DIRECTIONAL_LIGHT, spot = object.type == OBJECT_SPOT_LIGHT;
        cy::Vec3f direction = -object.rotation.Column(2);
        cy::Vec3f p = directional ? -direction : object.position;       // a directional light stores the direction toward it
        cy::Vec3f color = PALETTE[object.color] * object.intensity;
        float values[12] = { p.x, p.y, p.z, directional ? 0.0f : 1.0f,
                             direction.x, direction.y, direction.z, spot ? std::cos(object.cone * PI / 180) : -2.0f,
                             color.x, color.y, color.z, object.reach };
        std::copy(values, values + 12, block.lights[block.count++]);
    }
    return block;
}
