#define CGLTF_IMPLEMENTATION
#include "gltf_loader.h"

#include <cgltf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <wincodec.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;
using Float3 = std::array<float, 3>;

// Textures keep their native resolution; only very large ones are downscaled.
constexpr uint32_t kMaxTextureSize = 2048;

void logGltf(const std::string& message) {
    OutputDebugStringA((message + "\n").c_str());
    std::ofstream file("Vulkan-Playground.log", std::ios::app);
    if (file.is_open()) {
        file << message << '\n';
    }
}

struct GltfGuard {
    cgltf_data* data = nullptr;
    ~GltfGuard() {
        if (data) {
            cgltf_free(data);
        }
    }
};

bool decodeImageToTexture(
    const uint8_t* data,
    size_t size,
    minitracer::TextureData& out) {
    if (!data || size == 0) {
        return false;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)))) {
        return false;
    }

    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream))) {
        return false;
    }
    if (FAILED(stream->InitializeFromMemory(
            const_cast<BYTE*>(data), static_cast<DWORD>(size)))) {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(
            stream.Get(),
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder))) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) {
        return false;
    }
    if (FAILED(converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom))) {
        return false;
    }

    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    if (FAILED(frame->GetSize(&sourceWidth, &sourceHeight)) ||
        sourceWidth == 0 || sourceHeight == 0) {
        return false;
    }

    // Downscale only when the native resolution exceeds the cap, keeping aspect.
    const uint32_t largest = std::max(sourceWidth, sourceHeight);
    const float scale = largest > kMaxTextureSize
                            ? static_cast<float>(kMaxTextureSize) /
                                  static_cast<float>(largest)
                            : 1.0f;
    const uint32_t targetWidth = static_cast<uint32_t>(
        std::max(1.0f, std::floor(static_cast<float>(sourceWidth) * scale)));
    const uint32_t targetHeight = static_cast<uint32_t>(
        std::max(1.0f, std::floor(static_cast<float>(sourceHeight) * scale)));

    out.width = targetWidth;
    out.height = targetHeight;
    out.pixels.resize(static_cast<size_t>(targetWidth) * targetHeight * 4);
    const UINT stride = targetWidth * 4;
    const UINT bufferSize = static_cast<UINT>(out.pixels.size());

    if (targetWidth == sourceWidth && targetHeight == sourceHeight) {
        return SUCCEEDED(converter->CopyPixels(
            nullptr, stride, bufferSize, out.pixels.data()));
    }

    ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(&scaler))) {
        return false;
    }
    if (FAILED(scaler->Initialize(
            converter.Get(),
            targetWidth,
            targetHeight,
            WICBitmapInterpolationModeFant))) {
        return false;
    }

    return SUCCEEDED(scaler->CopyPixels(
        nullptr, stride, bufferSize, out.pixels.data()));
}

// Resamples one decoded layer in place. Used to give every layer of a texture
// array the same extent, which Vulkan requires.
bool resampleTexture(
    minitracer::TextureData& texture,
    uint32_t targetWidth,
    uint32_t targetHeight) {
    if (texture.width == targetWidth && texture.height == targetHeight) {
        return true;
    }
    if (texture.pixels.empty() || targetWidth == 0 || targetHeight == 0) {
        return false;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)))) {
        return false;
    }

    ComPtr<IWICBitmap> bitmap;
    if (FAILED(factory->CreateBitmapFromMemory(
            texture.width,
            texture.height,
            GUID_WICPixelFormat32bppRGBA,
            texture.width * 4,
            static_cast<UINT>(texture.pixels.size()),
            texture.pixels.data(),
            &bitmap))) {
        return false;
    }

    ComPtr<IWICBitmapScaler> scaler;
    if (FAILED(factory->CreateBitmapScaler(&scaler)) ||
        FAILED(scaler->Initialize(
            bitmap.Get(),
            targetWidth,
            targetHeight,
            WICBitmapInterpolationModeFant))) {
        return false;
    }

    minitracer::TextureData resized;
    resized.width = targetWidth;
    resized.height = targetHeight;
    resized.pixels.resize(static_cast<size_t>(targetWidth) * targetHeight * 4);
    const UINT stride = targetWidth * 4;
    const UINT bufferSize = static_cast<UINT>(resized.pixels.size());
    if (FAILED(scaler->CopyPixels(
            nullptr, stride, bufferSize, resized.pixels.data()))) {
        return false;
    }

    texture = std::move(resized);
    return true;
}

bool decodeImageFileToTexture(
    const std::filesystem::path& path,
    minitracer::TextureData& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    const auto size = static_cast<size_t>(file.tellg());
    std::vector<uint8_t> data(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return decodeImageToTexture(data.data(), data.size(), out);
}

// ---------------------------------------------------------------------------
// glTF texture state: array layers, per-texture sampler behaviour and
// KHR_texture_transform, all deduplicated into lookup tables on the mesh.
// ---------------------------------------------------------------------------

struct TextureContext {
    cgltf_data* data = nullptr;
    minitracer::Mesh* mesh = nullptr;
    std::map<std::pair<size_t, int>, int> layers;   // (image, srgb) -> array layer
    std::map<std::tuple<int, int, int, int, int, int, int, int>, int> slots;
    std::map<std::tuple<float, float, float, float, float, int>, int> transforms;
    bool warnedTexCoord = false;
};

int wrapModeValue(cgltf_wrap_mode mode) {
    if (mode == cgltf_wrap_mode_clamp_to_edge) {
        return 1;
    }
    if (mode == cgltf_wrap_mode_mirrored_repeat) {
        return 2;
    }
    return 0;  // repeat
}

const char* wrapModeName(int mode) {
    return mode == 1 ? "clamp" : (mode == 2 ? "mirror" : "repeat");
}

int magFilterValue(cgltf_filter_type filter) {
    return filter == cgltf_filter_type_nearest ? 0 : 1;
}

// 0 nearest, 1 linear (no mip), 2 trilinear over the mip chain.
int minFilterValue(cgltf_filter_type filter) {
    if (filter == cgltf_filter_type_nearest) {
        return 0;
    }
    if (filter == cgltf_filter_type_nearest_mipmap_linear ||
        filter == cgltf_filter_type_linear_mipmap_linear ||
        filter == cgltf_filter_type_undefined) {
        return 2;
    }
    return 1;
}

const char* filterName(int value) {
    return value == 0 ? "nearest" : (value == 1 ? "linear" : "linearMip");
}

void applySampler(const cgltf_texture* texture, minitracer::TextureSlot& slot) {
    if (!texture || !texture->sampler) {
        return;
    }
    slot.wrapS = wrapModeValue(texture->sampler->wrap_s);
    slot.wrapT = wrapModeValue(texture->sampler->wrap_t);
    slot.magFilter = magFilterValue(texture->sampler->mag_filter);
    slot.minFilter = minFilterValue(texture->sampler->min_filter);
}

int ensureTransform(
    TextureContext& context,
    const cgltf_texture_view& view,
    int& texCoord) {
    texCoord = view.texcoord;
    if (!view.has_transform) {
        return -1;
    }

    const cgltf_texture_transform& source = view.transform;
    if (source.has_texcoord) {
        texCoord = source.texcoord;
    }

    const auto key = std::make_tuple(
        static_cast<float>(source.offset[0]),
        static_cast<float>(source.offset[1]),
        static_cast<float>(source.rotation),
        static_cast<float>(source.scale[0]),
        static_cast<float>(source.scale[1]),
        texCoord);
    const auto existing = context.transforms.find(key);
    if (existing != context.transforms.end()) {
        return existing->second;
    }

    minitracer::TextureTransform transform;
    transform.offset[0] = static_cast<float>(source.offset[0]);
    transform.offset[1] = static_cast<float>(source.offset[1]);
    transform.rotation = static_cast<float>(source.rotation);
    transform.scale[0] = static_cast<float>(source.scale[0]);
    transform.scale[1] = static_cast<float>(source.scale[1]);

    const int index = static_cast<int>(context.mesh->texTransforms.size());
    context.mesh->texTransforms.push_back(transform);
    context.transforms[key] = index;
    logGltf("Texture transform " + std::to_string(index) +
            ": offset=(" + std::to_string(transform.offset[0]) + ", " +
            std::to_string(transform.offset[1]) + ") rotation=" +
            std::to_string(transform.rotation) + " scale=(" +
            std::to_string(transform.scale[0]) + ", " +
            std::to_string(transform.scale[1]) + ")");
    return index;
}

int ensureLayer(TextureContext& context, const cgltf_texture* texture, bool srgb) {
    if (!texture || !texture->image) {
        return -1;
    }

    const size_t imageIndex =
        static_cast<size_t>(texture->image - context.data->images);
    const auto key = std::make_pair(imageIndex, srgb ? 1 : 0);
    const auto existing = context.layers.find(key);
    if (existing != context.layers.end()) {
        return existing->second;
    }

    const cgltf_image* image = texture->image;
    minitracer::TextureData decoded;
    bool ok = false;
    if (image->buffer_view && image->buffer_view->buffer &&
        image->buffer_view->buffer->data) {
        const auto* bytes =
            static_cast<const uint8_t*>(image->buffer_view->buffer->data) +
            image->buffer_view->offset;
        ok = decodeImageToTexture(bytes, image->buffer_view->size, decoded);
    } else if (image->uri && std::strncmp(image->uri, "data:", 5) != 0) {
        ok = decodeImageFileToTexture(
            std::filesystem::u8path(image->uri), decoded);
    }

    if (!ok) {
        logGltf("Failed to decode glTF image index=" +
                std::to_string(imageIndex));
        return -1;
    }

    std::vector<minitracer::TextureData>& group =
        srgb ? context.mesh->texturesSrgb : context.mesh->texturesLinear;
    const int layer = static_cast<int>(group.size());
    logGltf("Decoded glTF image index=" + std::to_string(imageIndex) +
            (srgb ? " as sRGB texture" : " as linear texture") +
            " size=" + std::to_string(decoded.width) + "x" +
            std::to_string(decoded.height) + " -> layer=" +
            std::to_string(layer));
    group.push_back(std::move(decoded));
    context.layers[key] = layer;
    return layer;
}

int ensureSlot(
    TextureContext& context,
    const cgltf_texture_view& view,
    bool srgb) {
    if (!view.texture) {
        return -1;
    }

    int texCoord = 0;
    const int transformIndex = ensureTransform(context, view, texCoord);
    const int layer = ensureLayer(context, view.texture, srgb);
    if (layer < 0) {
        return -1;
    }
    if (texCoord != 0 && !context.warnedTexCoord) {
        logGltf("Warning: TEXCOORD_1 textures are unsupported, using TEXCOORD_0.");
        context.warnedTexCoord = true;
    }

    minitracer::TextureSlot slot;
    slot.layer = layer;
    slot.transformIndex = transformIndex;
    slot.texCoord = 0;
    applySampler(view.texture, slot);

    const auto key = std::make_tuple(
        layer,
        srgb ? 1 : 0,
        slot.wrapS,
        slot.wrapT,
        slot.magFilter,
        slot.minFilter,
        transformIndex,
        slot.texCoord);
    const auto existing = context.slots.find(key);
    if (existing != context.slots.end()) {
        return existing->second;
    }

    const int index = static_cast<int>(context.mesh->texSlots.size());
    context.mesh->texSlots.push_back(slot);
    context.slots[key] = index;
    logGltf("Texture slot " + std::to_string(index) +
            ": layer=" + std::to_string(layer) +
            (srgb ? " srgb" : " linear") +
            " wrapS=" + wrapModeName(slot.wrapS) +
            " wrapT=" + wrapModeName(slot.wrapT) +
            " mag=" + filterName(slot.magFilter) +
            " min=" + filterName(slot.minFilter) +
            " transform=" + std::to_string(transformIndex));
    return index;
}

// A texture array needs one extent for all layers, so layers that differ are
// resampled up to the largest extent of their group.
void normalizeGroup(std::vector<minitracer::TextureData>& group, const char* name) {
    uint32_t width = 0;
    uint32_t height = 0;
    for (const auto& texture : group) {
        width = std::max(width, texture.width);
        height = std::max(height, texture.height);
    }
    for (auto& texture : group) {
        if (texture.width == width && texture.height == height) {
            continue;
        }
        if (resampleTexture(texture, width, height)) {
            logGltf(std::string("Resampled ") + name + " texture layer to " +
                    std::to_string(width) + "x" + std::to_string(height));
        }
    }
}

Float3 transformPoint(const float* m, const Float3& p) {
    return {
        m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
        m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
        m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}

Float3 transformDirection(const float* m, const Float3& v) {
    return {
        m[0] * v[0] + m[4] * v[1] + m[8] * v[2],
        m[1] * v[0] + m[5] * v[1] + m[9] * v[2],
        m[2] * v[0] + m[6] * v[1] + m[10] * v[2]};
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

Float3 cross(const Float3& a, const Float3& b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0]};
}

Float3 subtract(const Float3& a, const Float3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

} // namespace

namespace minitracer {

Mesh loadGltf(const std::filesystem::path& path) {
    const std::string pathString = path.u8string();

    cgltf_options options{};
    GltfGuard guard;
    if (cgltf_parse_file(&options, pathString.c_str(), &guard.data) != cgltf_result_success) {
        throw std::runtime_error("Failed to parse glTF file: " + pathString);
    }
    if (cgltf_load_buffers(&options, guard.data, pathString.c_str()) != cgltf_result_success) {
        throw std::runtime_error("Failed to load glTF buffers: " + pathString);
    }

    cgltf_data* data = guard.data;
    Mesh mesh;
    TextureContext textures;
    textures.data = data;
    textures.mesh = &mesh;

    logGltf("glTF images total=" + std::to_string(data->images_count) +
            ", samplers=" + std::to_string(data->samplers_count));

    for (size_t i = 0; i < data->materials_count; ++i) {
        const cgltf_material& src = data->materials[i];
        Material material{};
        if (src.has_pbr_metallic_roughness) {
            material.baseColor[0] = src.pbr_metallic_roughness.base_color_factor[0];
            material.baseColor[1] = src.pbr_metallic_roughness.base_color_factor[1];
            material.baseColor[2] = src.pbr_metallic_roughness.base_color_factor[2];
            material.baseColorAlpha =
                src.pbr_metallic_roughness.base_color_factor[3];
            material.metallic = src.pbr_metallic_roughness.metallic_factor;
            material.roughness = src.pbr_metallic_roughness.roughness_factor;
            material.baseColorSlot = ensureSlot(
                textures,
                src.pbr_metallic_roughness.base_color_texture,
                true);
            material.metallicRoughnessSlot = ensureSlot(
                textures,
                src.pbr_metallic_roughness.metallic_roughness_texture,
                false);
        }
        material.transmission = src.has_transmission
                                    ? src.transmission.transmission_factor
                                    : 0.0f;
        material.ior = src.has_ior ? src.ior.ior : 1.5f;
        material.alphaMode = static_cast<int32_t>(src.alpha_mode);
        material.alphaCutoff = src.alpha_cutoff;
        material.doubleSided = src.double_sided ? 1 : 0;
        material.emissiveStrength =
            src.has_emissive_strength
                ? src.emissive_strength.emissive_strength
                : 1.0f;
        material.specularFactor =
            src.has_specular ? src.specular.specular_factor : 1.0f;
        material.specularColorFactor[0] =
            src.has_specular ? src.specular.specular_color_factor[0] : 1.0f;
        material.specularColorFactor[1] =
            src.has_specular ? src.specular.specular_color_factor[1] : 1.0f;
        material.specularColorFactor[2] =
            src.has_specular ? src.specular.specular_color_factor[2] : 1.0f;
        material.clearcoatFactor =
            src.has_clearcoat ? src.clearcoat.clearcoat_factor : 0.0f;
        material.clearcoatRoughness =
            src.has_clearcoat ? src.clearcoat.clearcoat_roughness_factor : 0.0f;

        if (src.has_volume) {
            material.thickness = src.volume.thickness_factor;
            material.attenuationColor[0] = src.volume.attenuation_color[0];
            material.attenuationColor[1] = src.volume.attenuation_color[1];
            material.attenuationColor[2] = src.volume.attenuation_color[2];
            material.attenuationDistance = src.volume.attenuation_distance;
            material.thicknessSlot =
                ensureSlot(textures, src.volume.thickness_texture, false);
        }
        if (material.attenuationDistance <= 0.0f) {
            material.attenuationDistance = 1e30f;
        }

        logGltf("Material " + std::to_string(i) +
                ": transmission=" + std::to_string(material.transmission) +
                " ior=" + std::to_string(material.ior) +
                " thickness=" + std::to_string(material.thickness) +
                " attenuationDistance=" +
                std::to_string(material.attenuationDistance));
        material.normalSlot = ensureSlot(textures, src.normal_texture, false);
        material.normalScale = src.normal_texture.scale;
        material.occlusionSlot =
            ensureSlot(textures, src.occlusion_texture, false);
        material.emissiveSlot =
            ensureSlot(textures, src.emissive_texture, true);
        material.emissive[0] = src.emissive_factor[0];
        material.emissive[1] = src.emissive_factor[1];
        material.emissive[2] = src.emissive_factor[2];
        mesh.materials.push_back(material);
    }

    if (mesh.materials.empty()) {
        mesh.materials.push_back(Material{});
    }

    normalizeGroup(mesh.texturesSrgb, "sRGB");
    normalizeGroup(mesh.texturesLinear, "linear");

    logGltf("Texture arrays: srgb layers=" +
            std::to_string(mesh.texturesSrgb.size()) + " linear layers=" +
            std::to_string(mesh.texturesLinear.size()) + " slots=" +
            std::to_string(mesh.texSlots.size()) + " transforms=" +
            std::to_string(mesh.texTransforms.size()));

    for (size_t nodeIndex = 0; nodeIndex < data->nodes_count; ++nodeIndex) {
        const cgltf_node* node = &data->nodes[nodeIndex];
        if (!node->mesh) {
            continue;
        }

        float worldMatrix[16];
        cgltf_node_transform_world(node, worldMatrix);

        const cgltf_mesh* gltfMesh = node->mesh;
        for (size_t primitiveIndex = 0;
             primitiveIndex < gltfMesh->primitives_count;
             ++primitiveIndex) {
            const cgltf_primitive& primitive = gltfMesh->primitives[primitiveIndex];
            if (primitive.type != cgltf_primitive_type_triangles) {
                continue;
            }

            const cgltf_accessor* positionAccessor = nullptr;
            const cgltf_accessor* normalAccessor = nullptr;
            const cgltf_accessor* uvAccessor = nullptr;

            for (size_t attributeIndex = 0;
                 attributeIndex < primitive.attributes_count;
                 ++attributeIndex) {
                const auto& attribute = primitive.attributes[attributeIndex];
                if (attribute.type == cgltf_attribute_type_position) {
                    positionAccessor = attribute.data;
                } else if (attribute.type == cgltf_attribute_type_normal) {
                    normalAccessor = attribute.data;
                } else if (attribute.type == cgltf_attribute_type_texcoord &&
                           attribute.index == 0) {
                    uvAccessor = attribute.data;
                }
            }

            if (!positionAccessor) {
                continue;
            }

            const uint32_t baseVertex =
                static_cast<uint32_t>(mesh.vertices.size());
            const size_t vertexCount = positionAccessor->count;

            for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
                float p[3] = {0.0f, 0.0f, 0.0f};
                cgltf_accessor_read_float(
                    positionAccessor, vertexIndex, p, 3);

                Vertex vertex{};
                const Float3 worldPosition = transformPoint(
                    worldMatrix, {p[0], p[1], p[2]});
                vertex.position[0] = worldPosition[0];
                vertex.position[1] = worldPosition[1];
                vertex.position[2] = worldPosition[2];

                if (normalAccessor) {
                    float n[3] = {0.0f, 0.0f, 1.0f};
                    cgltf_accessor_read_float(normalAccessor, vertexIndex, n, 3);
                    Float3 worldNormal = transformDirection(
                        worldMatrix, {n[0], n[1], n[2]});
                    normalize(worldNormal);
                    vertex.normal[0] = worldNormal[0];
                    vertex.normal[1] = worldNormal[1];
                    vertex.normal[2] = worldNormal[2];
                } else {
                    vertex.normal[0] = 0.0f;
                    vertex.normal[1] = 0.0f;
                    vertex.normal[2] = 0.0f;
                }

                if (uvAccessor) {
                    float uv[2] = {0.0f, 0.0f};
                    cgltf_accessor_read_float(uvAccessor, vertexIndex, uv, 2);
                    vertex.uv[0] = uv[0];
                    vertex.uv[1] = uv[1];
                }

                mesh.vertices.push_back(vertex);
            }

            uint32_t materialIndex = 0;
            if (primitive.material) {
                materialIndex = static_cast<uint32_t>(
                    primitive.material - data->materials);
            }

            const size_t indexCount = primitive.indices
                                          ? primitive.indices->count
                                          : vertexCount;
            for (size_t i = 0; i + 2 < indexCount; i += 3) {
                const uint32_t i0 = primitive.indices
                                        ? static_cast<uint32_t>(
                                              cgltf_accessor_read_index(
                                                  primitive.indices, i))
                                        : static_cast<uint32_t>(i);
                const uint32_t i1 = primitive.indices
                                        ? static_cast<uint32_t>(
                                              cgltf_accessor_read_index(
                                                  primitive.indices, i + 1))
                                        : static_cast<uint32_t>(i + 1);
                const uint32_t i2 = primitive.indices
                                        ? static_cast<uint32_t>(
                                              cgltf_accessor_read_index(
                                                  primitive.indices, i + 2))
                                        : static_cast<uint32_t>(i + 2);

                Triangle triangle{};
                triangle.v0 = baseVertex + i0;
                triangle.v1 = baseVertex + i1;
                triangle.v2 = baseVertex + i2;
                triangle.materialIndex = materialIndex;
                mesh.triangles.push_back(triangle);
            }
        }
    }

    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        throw std::runtime_error("glTF file contains no triangle geometry.");
    }

    for (const auto& triangle : mesh.triangles) {
        Vertex& a = mesh.vertices[triangle.v0];
        Vertex& b = mesh.vertices[triangle.v1];
        Vertex& c = mesh.vertices[triangle.v2];
        Float3 faceNormal = cross(
            subtract({b.position[0], b.position[1], b.position[2]},
                     {a.position[0], a.position[1], a.position[2]}),
            subtract({c.position[0], c.position[1], c.position[2]},
                     {a.position[0], a.position[1], a.position[2]}));
        normalize(faceNormal);

        for (Vertex* vertex : {&a, &b, &c}) {
            Float3 n = {vertex->normal[0], vertex->normal[1], vertex->normal[2]};
            if (length(n) < 1e-8f) {
                vertex->normal[0] = faceNormal[0];
                vertex->normal[1] = faceNormal[1];
                vertex->normal[2] = faceNormal[2];
            }
        }
    }

    float boundsMin[3] = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    float boundsMax[3] = {
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()};

    for (const auto& vertex : mesh.vertices) {
        for (int i = 0; i < 3; ++i) {
            boundsMin[i] = std::min(boundsMin[i], vertex.position[i]);
            boundsMax[i] = std::max(boundsMax[i], vertex.position[i]);
        }
    }

    const float center[3] = {
        (boundsMin[0] + boundsMax[0]) * 0.5f,
        (boundsMin[1] + boundsMax[1]) * 0.5f,
        (boundsMin[2] + boundsMax[2]) * 0.5f};
    const float extent[3] = {
        boundsMax[0] - boundsMin[0],
        boundsMax[1] - boundsMin[1],
        boundsMax[2] - boundsMin[2]};
    const float maxExtent = std::max({extent[0], extent[1], extent[2]});
    const float scale = maxExtent > 1e-6f ? 0.8f / maxExtent : 1.0f;

    for (auto& vertex : mesh.vertices) {
        vertex.position[0] = (vertex.position[0] - center[0]) * scale;
        vertex.position[1] = (vertex.position[1] - center[1]) * scale;
        vertex.position[2] = (vertex.position[2] - center[2]) * scale;

        Float3 n = {vertex.normal[0], vertex.normal[1], vertex.normal[2]};
        normalize(n);
        vertex.normal[0] = n[0];
        vertex.normal[1] = n[1];
        vertex.normal[2] = n[2];
    }

    for (auto& material : mesh.materials) {
        if (material.thickness > 0.0f) {
            material.thickness *= scale;
        }
        if (material.attenuationDistance < 1e29f) {
            material.attenuationDistance *= scale;
        }
    }

    return mesh;
}

} // namespace minitracer