#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace minitracer {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vertex {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 1.0f};
    float uv[2] = {0.0f, 0.0f};
};

struct Triangle {
    uint32_t v0 = 0;
    uint32_t v1 = 0;
    uint32_t v2 = 0;
    uint32_t materialIndex = 0;
};

// One sampled texture reference: array layer, wrap/filter behaviour and the
// KHR_texture_transform that applies to it. Slot index -1 means "no texture".
struct TextureSlot {
    int32_t layer = -1;          // layer inside the sRGB or the linear array
    int32_t wrapS = 0;           // 0 repeat, 1 clamp to edge, 2 mirrored repeat
    int32_t wrapT = 0;
    int32_t magFilter = 1;       // 0 nearest, 1 linear
    int32_t minFilter = 2;       // 0 nearest, 1 linear, 2 linear + mip chain
    int32_t transformIndex = -1; // index into Mesh::texTransforms, -1 = identity
    int32_t texCoord = 0;        // only TEXCOORD_0 is supported
    int32_t _pad = 0;
};

// KHR_texture_transform: uv' = offset + rotation * (scale * uv)
struct TextureTransform {
    float offset[2] = {0.0f, 0.0f};
    float rotation = 0.0f;
    float scale[2] = {1.0f, 1.0f};
    float _pad[3] = {0.0f, 0.0f, 0.0f};
};

struct Material {
    float baseColor[3] = {1.0f, 1.0f, 1.0f};
    int32_t baseColorSlot = -1;
    float metallic = 0.0f;
    float roughness = 0.5f;
    float transmission = 0.0f;
    int32_t metallicRoughnessSlot = -1;
    int32_t normalSlot = -1;
    int32_t occlusionSlot = -1;
    int32_t emissiveSlot = -1;
    float emissive[3] = {0.0f, 0.0f, 0.0f};
    float ior = 1.5f;
    float thickness = 0.0f;
    float attenuationColor[3] = {1.0f, 1.0f, 1.0f};
    float attenuationDistance = 0.0f;
    int32_t thicknessSlot = -1;
    float baseColorAlpha = 1.0f;
    int32_t alphaMode = 0;
    float alphaCutoff = 0.5f;
    int32_t doubleSided = 0;
    float emissiveStrength = 1.0f;
    float specularFactor = 1.0f;
    float specularColorFactor[3] = {1.0f, 1.0f, 1.0f};
    float clearcoatFactor = 0.0f;
    float clearcoatRoughness = 0.0f;
    float normalScale = 1.0f;
    int32_t flipUvY = 0;
};

struct TextureData {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels;
};

struct GPULight {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float direction[3] = {0.0f, -1.0f, 0.0f};
    float color[3] = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    int32_t type = 0;
    float spotDirection[3] = {0.0f, -1.0f, 0.0f};
    float spotInnerCos = 0.9f;
    float spotOuterCos = 0.8f;
    float areaRight[3] = {1.0f, 0.0f, 0.0f};
    float areaUp[3] = {0.0f, 0.0f, 1.0f};
    float areaHalfWidth = 1.0f;
    float areaHalfHeight = 1.0f;
    float _pad[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct BVHNode {
    float min[3] = {0.0f, 0.0f, 0.0f};
    float max[3] = {0.0f, 0.0f, 0.0f};
    int32_t left = -1;
    int32_t right = -1;
    int32_t start = 0;
    int32_t count = 0;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::vector<Material> materials;
    // glTF textures are split by colour space: sRGB arrays for baseColor and
    // emissive, linear arrays for metallicRoughness, normal, occlusion, thickness.
    std::vector<TextureData> texturesSrgb;
    std::vector<TextureData> texturesLinear;
    std::vector<TextureSlot> texSlots;
    std::vector<TextureTransform> texTransforms;
    std::vector<BVHNode> bvhNodes;
    std::vector<std::string> materialTexturePaths;
};

static_assert(sizeof(Vec2) == 8, "Vec2 must be 8 bytes for GPU buffers.");
static_assert(sizeof(Vec3) == 12, "Vec3 must be 12 bytes for GPU buffers.");
static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes for GPU buffers.");
static_assert(sizeof(Triangle) == 16, "Triangle must be 16 bytes for GPU buffers.");
static_assert(sizeof(Material) == 136, "Material must be 136 bytes for GPU buffers.");
static_assert(sizeof(TextureSlot) == 32, "TextureSlot must be 32 bytes for GPU buffers.");
static_assert(sizeof(TextureTransform) == 32, "TextureTransform must be 32 bytes for GPU buffers.");
static_assert(sizeof(BVHNode) == 40, "BVHNode must be 40 bytes for GPU buffers.");
static_assert(sizeof(GPULight) == 112, "GPULight must be 112 bytes for GPU buffers.");

inline Mesh make_demo_triangle() {
    Mesh mesh;

    Vertex v0;
    v0.position[0] = -0.5f;
    v0.position[1] = -0.5f;
    v0.position[2] = 0.0f;
    v0.uv[0] = 0.0f;
    v0.uv[1] = 1.0f;

    Vertex v1;
    v1.position[0] = 0.5f;
    v1.position[1] = -0.5f;
    v1.position[2] = 0.0f;
    v1.uv[0] = 1.0f;
    v1.uv[1] = 1.0f;

    Vertex v2;
    v2.position[0] = 0.0f;
    v2.position[1] = 0.5f;
    v2.position[2] = 0.0f;
    v2.uv[0] = 0.5f;
    v2.uv[1] = 0.0f;

    mesh.vertices = {v0, v1, v2};
    mesh.triangles.push_back(Triangle{0, 1, 2, 0});
    mesh.materials.push_back(Material{});
    return mesh;
}

} // namespace minitracer
