#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cyVector.h>

struct SplatData {
    int count = 0;
    std::vector<float>    positions;    // 4 per splat: mean xyz, opacity
    std::vector<float>    covariances;  // 6 per splat: xx xy xz yy yz zz
    std::vector<uint32_t> sh;           // 24 per splat: 16 RGB coefficients as 48 half floats
    cy::Vec3f center, up;               // center of the dense part of the scene, estimated up direction
    float radius = 1, extent = 1;       // distances from the center that contain 80% and 100% of the splats
};

// Converts a float to a 16-bit half float, rounded to nearest. Too small becomes 0, too large the largest half.
uint16_t floatToHalf(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    uint32_t sign = (bits >> 16) & 0x8000u;         // sign bit
    int exponent = int((bits >> 23) & 0xFFu) - 127 + 15;    // exponent bits
    uint32_t mantissa = bits & 0x7FFFFFu;                   // mantissa bits
    if (exponent <= 0) return uint16_t(sign);
    if (exponent >= 31) return uint16_t(sign | 0x7BFFu);
    return uint16_t((sign | (uint32_t(exponent) << 10) | (mantissa >> 13)) + ((mantissa >> 12) & 1u));
}

// Estimates the scene center, size and up direction from the splat positions
void estimateFrame(SplatData& splats) {
    int n = splats.count;
    const float* p = splats.positions.data();

    // median of each coordinate: a first center that ignores the far background
    std::vector<float> values(n);
    cy::Vec3f median;
    for (int axis = 0; axis < 3; axis++) {
        for (int i = 0; i < n; i++) values[i] = p[4 * i + axis];
        std::nth_element(values.begin(), values.begin() + n / 2, values.end());
        median[axis] = values[n / 2];
    }

    // radii around the median that contain 40% and 80% of the splats
    std::vector<float> distance(n);
    for (int i = 0; i < n; i++) distance[i] = (cy::Vec3f(p + 4 * i) - median).Length();
    values = distance;
    std::nth_element(values.begin(), values.begin() + n / 5 * 2, values.end());
    float innerRadius = values[n / 5 * 2];
    std::nth_element(values.begin(), values.begin() + n / 5 * 4, values.end());
    splats.radius = values[n / 5 * 4];

    // opacity-weighted mean and covariance of the inner splats
    double weight = 0, mean[3] = {}, cov[3][3] = {};
    for (int i = 0; i < n; i++) if (distance[i] < innerRadius) {
        weight += p[4 * i + 3];
        for (int a = 0; a < 3; a++) mean[a] += p[4 * i + 3] * p[4 * i + a];
    }
    for (int a = 0; a < 3; a++) mean[a] /= weight;
    for (int i = 0; i < n; i++) if (distance[i] < innerRadius)
        for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++)
            cov[a][b] += p[4 * i + 3] * (p[4 * i + a] - mean[a]) * (p[4 * i + b] - mean[b]);
    splats.center = cy::Vec3f((float)mean[0], (float)mean[1], (float)mean[2]);

    // The ground is flat, so up is the direction of least variance: the smallest eigenvector of cov,
    // which is the largest of (trace * I - cov) and is found by power iteration.
    double trace = cov[0][0] + cov[1][1] + cov[2][2];
    double up[3] = { 1, 1, 1 };
    for (int iteration = 0; iteration < 200; iteration++) {
        double next[3], length = 0;
        for (int a = 0; a < 3; a++) {
            next[a] = trace * up[a] - (cov[a][0] * up[0] + cov[a][1] * up[1] + cov[a][2] * up[2]);
            length += next[a] * next[a];
        }
        for (int a = 0; a < 3; a++) up[a] = next[a] / std::sqrt(length);
    }
    splats.up = cy::Vec3f((float)up[0], (float)up[1], (float)up[2]);

    // more of the scene lies above the ground than below it, so the heights are skewed toward up
    double skew = 0;
    splats.extent = 0;
    for (int i = 0; i < n; i++) {
        cy::Vec3f d = cy::Vec3f(p + 4 * i) - splats.center;
        splats.extent = std::max(splats.extent, d.Length());
        double height = d % splats.up;
        if (distance[i] < splats.radius) skew += height * height * height;
    }
    if (skew < 0) splats.up = -splats.up;
}

// Reads a 3DGS .ply file and converts each splat: sigmoid of the opacity, covariance from scale and rotation,
// half-float SH coefficients.
bool loadPly(const char* filename, SplatData& splats) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) { std::cerr << "Cannot open " << filename << std::endl; return false; }

    // header: number of splats and the names of the properties, in file order
    std::vector<std::string> names;
    std::string line, word, type;
    bool littleEndian = false, allFloat = true;
    while (std::getline(file, line) && line.rfind("end_header", 0) != 0) {
        std::istringstream words(line);
        words >> word;
        if (word == "format") { words >> word; littleEndian = word == "binary_little_endian"; }
        if (word == "element") words >> word >> splats.count;
        if (word == "property") { words >> type >> word; names.push_back(word); allFloat = allFloat && type == "float"; }
    }
    auto indexOf = [&](const char* name) { return int(std::find(names.begin(), names.end(), name) - names.begin()); };
    int stride = (int)names.size();     // floats per splat
    int ix = indexOf("x"), idc = indexOf("f_dc_0"), irest = indexOf("f_rest_0"), iopacity = indexOf("opacity");
    int iscale = indexOf("scale_0"), irot = indexOf("rot_0");
    if (!littleEndian || !allFloat || splats.count <= 0 || std::max({ ix, idc, iopacity, iscale, irot }) >= stride) {
        std::cerr << filename << " is not a 3DGS .ply file" << std::endl;
        return false;
    }
    int restCount = 0;                  // f_rest values per color channel: 15 for SH degree 3
    for (const std::string& name : names) if (name.rfind("f_rest_", 0) == 0) restCount++;
    restCount /= 3;

    size_t count = splats.count;
    splats.positions.resize(4 * count);
    splats.covariances.resize(6 * count);
    splats.sh.resize(24 * count);
    const size_t CHUNK = 65536;         // splats read from the file at a time
    std::vector<float> chunk(CHUNK * stride);
    for (size_t first = 0; first < count; first += CHUNK) {
        size_t n = std::min(CHUNK, count - first);
        file.read((char*)chunk.data(), n * stride * sizeof(float));
        if (!file) { std::cerr << filename << " is truncated" << std::endl; return false; }

        for (size_t k = 0; k < n; k++) {
            const float* v = &chunk[k * stride];
            size_t i = first + k;
            float* position = &splats.positions[4 * i];
            position[0] = v[ix];
            position[1] = v[ix + 1];
            position[2] = v[ix + 2];
            position[3] = 1.0f / (1.0f + std::exp(-v[iopacity]));

            // covariance = M M^T with M = R S: R from the normalized quaternion (w, x, y, z), S = diag(exp(scale))
            float w = v[irot], x = v[irot + 1], y = v[irot + 2], z = v[irot + 3];
            float norm = std::sqrt(w * w + x * x + y * y + z * z);
            w /= norm; x /= norm; y /= norm; z /= norm;
            float R[3][3] = { { 1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y) },
                              { 2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x) },
                              { 2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y) } };
            float M[3][3];
            for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) M[r][c] = R[r][c] * std::exp(v[iscale + c]);
            float* covariance = &splats.covariances[6 * i];
            for (int r = 0; r < 3; r++) for (int c = r; c < 3; c++)
                *covariance++ = M[r][0] * M[c][0] + M[r][1] * M[c][1] + M[r][2] * M[c][2];

            // SH: coefficient 0 is f_dc; f_rest holds the others as all red, then all green, then all blue
            uint16_t half[48] = {};
            for (int channel = 0; channel < 3; channel++) {
                half[channel] = floatToHalf(v[idc + channel]);
                for (int c = 0; c < std::min(restCount, 15); c++)
                    half[3 * (c + 1) + channel] = floatToHalf(v[irest + channel * restCount + c]);
            }
            for (int j = 0; j < 24; j++) splats.sh[24 * i + j] = half[2 * j] | (uint32_t(half[2 * j + 1]) << 16);
        }
    }
    estimateFrame(splats);
    return true;
}
