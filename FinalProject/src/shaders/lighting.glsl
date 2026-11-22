const int MAX_LIGHTS = 8;       // matches scene.h

struct Light {
    vec4 position;      // xyz: position and w = 1, or for a directional light the direction toward it and w = 0
    vec4 direction;     // xyz: direction of a spot light, w: cosine of its half angle (-2: not a spot light)
    vec4 color;         // rgb: tint times intensity, a: reach (where a point or spot light has that intensity)
};

layout(std140, binding = BLOCK_LIGHTS) uniform Lights {
    uint  lightCount;
    float ambient;      // scale of the light of the capture
    Light light[MAX_LIGHTS];
};

// Irradiance from one light at a point, relative to the sky of the capture. Also returns l, the direction toward the light.
vec3 irradiance(Light source, vec3 point, out vec3 l) {
    vec3 toLight = source.position.xyz - point * source.position.w;
    float d = length(toLight);
    l = toLight / d;
    if (source.position.w == 0.0) return source.color.rgb;                  // directional: the same everywhere

    float reach = source.color.a;
    float falloff = reach * reach / max(d * d, 0.01 * reach * reach);       // inverse square, 1 at the reach
    float cosCone = source.direction.w;                                     // spot: soft edge over the outer part of the cone
    if (cosCone > -1.0) falloff *= smoothstep(cosCone, mix(cosCone, 1.0, 0.36), dot(-l, source.direction.xyz));
    return source.color.rgb * falloff;
}

// Conversions between the display colors of the capture (sRGB) and linear light.
vec3 toLinear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

vec3 toSrgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}
