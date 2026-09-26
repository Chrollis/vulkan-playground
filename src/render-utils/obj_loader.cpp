#include "obj_loader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Float3 = std::array<float, 3>;
using Float2 = std::array<float, 2>;

struct FaceVertex {
    int position = -1;
    int uv = -1;
    int normal = -1;
};

struct MaterialBuilder {
    std::string name;
    Float3 baseColor = {0.85f, 0.85f, 0.85f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float transmission = 0.0f;
    float ior = 1.5f;
    std::string texturePath;
};

void parseFaceToken(
    const std::string& token,
    size_t positionCount,
    size_t uvCount,
    size_t normalCount,
    FaceVertex& out) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream stream(token);
    while (std::getline(stream, part, '/')) {
        parts.push_back(part);
    }

    auto parseIndex = [](const std::string& text, size_t count, int& result) {
        if (text.empty()) {
            result = -1;
            return;
        }
        int index = std::stoi(text);
        if (index < 0) {
            index = static_cast<int>(count) + index;
        } else {
            --index;
        }
        result = index;
    };

    if (!parts.empty()) {
        parseIndex(parts[0], positionCount, out.position);
    }
    if (parts.size() > 1) {
        parseIndex(parts[1], uvCount, out.uv);
    }
    if (parts.size() > 2) {
        parseIndex(parts[2], normalCount, out.normal);
    }
}

Float3 positionOf(const minitracer::Vertex& vertex) {
    return {vertex.position[0], vertex.position[1], vertex.position[2]};
}

Float3 normalOf(const minitracer::Vertex& vertex) {
    return {vertex.normal[0], vertex.normal[1], vertex.normal[2]};
}

Float3 subtract(const Float3& a, const Float3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Float3 cross(const Float3& a, const Float3& b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]};
}

float length(const Float3& v) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void normalize(Float3& v) {
    const float len = length(v);
    if (len > 1e-8f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

std::string restOfLine(std::istringstream& stream) {
    std::string rest;
    std::getline(stream, rest);
    while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
        rest.erase(rest.begin());
    }
    while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' ||
                             rest.back() == '\r')) {
        rest.pop_back();
    }
    return rest;
}

uint32_t ensureMaterial(
    const std::string& name,
    std::vector<MaterialBuilder>& materials,
    std::map<std::string, uint32_t>& indexByName) {
    const auto it = indexByName.find(name);
    if (it != indexByName.end()) {
        return it->second;
    }

    MaterialBuilder builder{};
    builder.name = name;
    const uint32_t index = static_cast<uint32_t>(materials.size());
    materials.push_back(builder);
    indexByName[name] = index;
    return index;
}

void parseMtl(
    const std::filesystem::path& mtlPath,
    const std::filesystem::path& baseDir,
    std::vector<MaterialBuilder>& materials,
    std::map<std::string, uint32_t>& indexByName) {
    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        return;
    }

    uint32_t current = 0;
    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        std::istringstream stream(line);
        std::string type;
        if (!(stream >> type)) {
            continue;
        }

        if (type == "newmtl") {
            const std::string name = restOfLine(stream);
            current = ensureMaterial(name, materials, indexByName);
        } else if (type == "Kd") {
            Float3 color{};
            stream >> color[0] >> color[1] >> color[2];
            materials[current].baseColor = color;
        } else if (type == "Ni") {
            stream >> materials[current].ior;
        } else if (type == "d") {
            float dissolve = 1.0f;
            stream >> dissolve;
            materials[current].transmission =
                std::clamp(1.0f - dissolve, 0.0f, 1.0f);
        } else if (type == "Ns") {
            float shininess = 0.0f;
            stream >> shininess;
            const float roughness =
                std::sqrt(2.0f / std::max(shininess + 2.0f, 1e-4f));
            materials[current].roughness =
                std::clamp(roughness, 0.04f, 1.0f);
        } else if (type == "map_Kd") {
            const std::string textureName = restOfLine(stream);
            if (!textureName.empty()) {
                const std::filesystem::path fullPath =
                    baseDir / std::filesystem::u8path(textureName);
                materials[current].texturePath = fullPath.u8string();
            }
        }
    }
}

} // namespace

namespace minitracer {

Mesh loadObj(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open OBJ file: " + path.string());
    }

    Mesh mesh;
    std::vector<Float3> positions;
    std::vector<Float2> texcoords;
    std::vector<Float3> normals;

    std::vector<MaterialBuilder> materialBuilders;
    std::map<std::string, uint32_t> materialIndexByName;
    uint32_t currentMaterial = 0;
    std::string mtlFileName;

    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        std::istringstream stream(line);
        std::string type;
        if (!(stream >> type)) {
            continue;
        }

        if (type == "v") {
            Float3 p{};
            stream >> p[0] >> p[1] >> p[2];
            positions.push_back(p);
        } else if (type == "vt") {
            Float2 uv{};
            stream >> uv[0] >> uv[1];
            texcoords.push_back(uv);
        } else if (type == "vn") {
            Float3 n{};
            stream >> n[0] >> n[1] >> n[2];
            normals.push_back(n);
        } else if (type == "mtllib") {
            mtlFileName = restOfLine(stream);
        } else if (type == "usemtl") {
            const std::string name = restOfLine(stream);
            currentMaterial = ensureMaterial(name, materialBuilders, materialIndexByName);
        } else if (type == "f") {
            std::vector<FaceVertex> face;
            std::string token;
            while (stream >> token) {
                FaceVertex fv;
                parseFaceToken(token, positions.size(), texcoords.size(), normals.size(), fv);
                if (fv.position < 0 || fv.position >= static_cast<int>(positions.size())) {
                    throw std::runtime_error("OBJ face references an invalid vertex.");
                }
                face.push_back(fv);
            }

            for (size_t i = 2; i < face.size(); ++i) {
                const FaceVertex corners[3] = {face[0], face[i - 1], face[i]};
                const size_t base = mesh.vertices.size();

                for (const auto& corner : corners) {
                    Vertex vertex{};
                    const auto& p = positions[corner.position];
                    vertex.position[0] = p[0];
                    vertex.position[1] = p[1];
                    vertex.position[2] = p[2];

                    if (corner.uv >= 0 && corner.uv < static_cast<int>(texcoords.size())) {
                        vertex.uv[0] = texcoords[corner.uv][0];
                        vertex.uv[1] = texcoords[corner.uv][1];
                    }

                    if (corner.normal >= 0 && corner.normal < static_cast<int>(normals.size())) {
                        vertex.normal[0] = normals[corner.normal][0];
                        vertex.normal[1] = normals[corner.normal][1];
                        vertex.normal[2] = normals[corner.normal][2];
                    } else {
                        vertex.normal[0] = 0.0f;
                        vertex.normal[1] = 0.0f;
                        vertex.normal[2] = 0.0f;
                    }

                    mesh.vertices.push_back(vertex);
                }

                Float3 faceNormal = cross(
                    subtract(positionOf(mesh.vertices[base + 1]), positionOf(mesh.vertices[base])),
                    subtract(positionOf(mesh.vertices[base + 2]), positionOf(mesh.vertices[base])));
                normalize(faceNormal);

                for (int k = 0; k < 3; ++k) {
                    Float3 n = normalOf(mesh.vertices[base + k]);
                    if (length(n) < 1e-8f) {
                        mesh.vertices[base + k].normal[0] = faceNormal[0];
                        mesh.vertices[base + k].normal[1] = faceNormal[1];
                        mesh.vertices[base + k].normal[2] = faceNormal[2];
                    }
                }

                Triangle triangle{};
                triangle.v0 = static_cast<uint32_t>(base);
                triangle.v1 = static_cast<uint32_t>(base + 1);
                triangle.v2 = static_cast<uint32_t>(base + 2);
                triangle.materialIndex = currentMaterial;
                mesh.triangles.push_back(triangle);
            }
        }
    }

    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        throw std::runtime_error("OBJ file contains no triangles.");
    }

    const std::filesystem::path baseDir = path.parent_path();
    if (!mtlFileName.empty()) {
        parseMtl(
            baseDir / std::filesystem::u8path(mtlFileName),
            baseDir,
            materialBuilders,
            materialIndexByName);
    }

    if (materialBuilders.empty()) {
        materialBuilders.push_back(MaterialBuilder{});
    }

    for (const auto& builder : materialBuilders) {
        Material material{};
        material.baseColor[0] = builder.baseColor[0];
        material.baseColor[1] = builder.baseColor[1];
        material.baseColor[2] = builder.baseColor[2];
        material.baseColorSlot = builder.texturePath.empty() ? -1 : 0;
        material.metallic = builder.metallic;
        material.roughness = builder.roughness;
        material.transmission = builder.transmission;
        material.ior = builder.ior;
        material.flipUvY = 1;
        mesh.materials.push_back(material);
        mesh.materialTexturePaths.push_back(builder.texturePath);
    }

    Float3 boundsMin = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    Float3 boundsMax = {
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()};

    for (const auto& vertex : mesh.vertices) {
        for (int i = 0; i < 3; ++i) {
            boundsMin[i] = std::min(boundsMin[i], vertex.position[i]);
            boundsMax[i] = std::max(boundsMax[i], vertex.position[i]);
        }
    }

    const Float3 center = {
        (boundsMin[0] + boundsMax[0]) * 0.5f,
        (boundsMin[1] + boundsMax[1]) * 0.5f,
        (boundsMin[2] + boundsMax[2]) * 0.5f};

    const Float3 extent = {
        boundsMax[0] - boundsMin[0],
        boundsMax[1] - boundsMin[1],
        boundsMax[2] - boundsMin[2]};
    const float maxExtent = std::max({extent[0], extent[1], extent[2]});
    const float scale = maxExtent > 1e-6f ? 0.8f / maxExtent : 1.0f;

    for (auto& vertex : mesh.vertices) {
        vertex.position[0] = (vertex.position[0] - center[0]) * scale;
        vertex.position[1] = (vertex.position[1] - center[1]) * scale;
        vertex.position[2] = (vertex.position[2] - center[2]) * scale;

        Float3 normal = normalOf(vertex);
        normalize(normal);
        vertex.normal[0] = normal[0];
        vertex.normal[1] = normal[1];
        vertex.normal[2] = normal[2];
    }

    return mesh;
}

} // namespace minitracer