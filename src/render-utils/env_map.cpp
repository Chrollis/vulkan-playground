#define STB_IMAGE_IMPLEMENTATION
#include "env_map.h"

#include "stb_image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

float radicalInverseVdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

std::array<float, 2> directionToUV(const std::array<float, 3>& dir) {
    const float u = std::atan2(dir[2], dir[0]) / (2.0f * kPi) + 0.5f;
    const float v = std::acos(std::clamp(dir[1], -1.0f, 1.0f)) / kPi;
    return {u, v};
}

std::array<float, 3> sampleBilinear(
    const std::vector<float>& pixels,
    uint32_t width,
    uint32_t height,
    const std::array<float, 3>& dir) {
    auto uv = directionToUV(dir);

    float u = uv[0] - std::floor(uv[0]);
    float v = std::clamp(uv[1], 0.0f, 1.0f);

    const float fx = u * static_cast<float>(width) - 0.5f;
    const float fy = v * static_cast<float>(height) - 0.5f;

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    auto fetch = [&](int x, int y) -> std::array<float, 3> {
        const int wx = ((x % static_cast<int>(width)) + static_cast<int>(width)) %
                       static_cast<int>(width);
        const int wy = std::clamp(y, 0, static_cast<int>(height) - 1);
        const size_t index = (static_cast<size_t>(wy) * width + wx) * 4;
        return {pixels[index + 0], pixels[index + 1], pixels[index + 2]};
    };

    const auto c00 = fetch(x0, y0);
    const auto c10 = fetch(x1, y0);
    const auto c01 = fetch(x0, y1);
    const auto c11 = fetch(x1, y1);

    std::array<float, 3> result{};
    for (int i = 0; i < 3; ++i) {
        const float top = c00[i] * (1.0f - tx) + c10[i] * tx;
        const float bottom = c01[i] * (1.0f - tx) + c11[i] * tx;
        result[i] = top * (1.0f - ty) + bottom * ty;
    }
    return result;
}

std::vector<std::vector<float>> buildMips(
    const std::vector<float>& base,
    uint32_t width,
    uint32_t height) {
    std::vector<std::vector<float>> mips;
    mips.push_back(base);

    uint32_t w = width;
    uint32_t h = height;
    while (w > 1 || h > 1) {
        const uint32_t nw = std::max(1u, w / 2u);
        const uint32_t nh = std::max(1u, h / 2u);
        const std::vector<float>& prev = mips.back();
        std::vector<float> next(static_cast<size_t>(nw) * nh * 4, 0.0f);

        for (uint32_t y = 0; y < nh; ++y) {
            for (uint32_t x = 0; x < nw; ++x) {
                float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                int count = 0;
                for (uint32_t dy = 0; dy < 2; ++dy) {
                    for (uint32_t dx = 0; dx < 2; ++dx) {
                        const uint32_t sx = std::min(w - 1u, x * 2u + dx);
                        const uint32_t sy = std::min(h - 1u, y * 2u + dy);
                        const size_t src = (static_cast<size_t>(sy) * w + sx) * 4;
                        for (int c = 0; c < 4; ++c) {
                            sum[c] += prev[src + c];
                        }
                        ++count;
                    }
                }
                const size_t dst = (static_cast<size_t>(y) * nw + x) * 4;
                for (int c = 0; c < 4; ++c) {
                    next[dst + c] = sum[c] / static_cast<float>(count);
                }
            }
        }

        mips.push_back(std::move(next));
        w = nw;
        h = nh;
    }
    return mips;
}

std::vector<float> buildIrradiance(
    const std::vector<float>& base,
    uint32_t width,
    uint32_t height,
    uint32_t irrWidth,
    uint32_t irrHeight) {
    std::vector<float> irradiance(
        static_cast<size_t>(irrWidth) * irrHeight * 4, 0.0f);

    constexpr int kSampleCount = 64;

    for (uint32_t y = 0; y < irrHeight; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) /
                        static_cast<float>(irrHeight);
        const float theta = v * kPi;
        const float sinTheta = std::sin(theta);

        for (uint32_t x = 0; x < irrWidth; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) /
                            static_cast<float>(irrWidth);
            const float phi = (u - 0.5f) * 2.0f * kPi;

            const std::array<float, 3> N = {
                sinTheta * std::cos(phi),
                std::cos(theta),
                sinTheta * std::sin(phi)};

            const std::array<float, 3> up =
                (std::abs(N[1]) < 0.999f)
                    ? std::array<float, 3>{0.0f, 1.0f, 0.0f}
                    : std::array<float, 3>{0.0f, 0.0f, 1.0f};

            const std::array<float, 3> right = [&]() {
                const float rx = up[1] * N[2] - up[2] * N[1];
                const float ry = up[2] * N[0] - up[0] * N[2];
                const float rz = up[0] * N[1] - up[1] * N[0];
                const float len = std::sqrt(rx * rx + ry * ry + rz * rz);
                return std::array<float, 3>{rx / len, ry / len, rz / len};
            }();

            const std::array<float, 3> tangent = {
                N[1] * right[2] - N[2] * right[1],
                N[2] * right[0] - N[0] * right[2],
                N[0] * right[1] - N[1] * right[0]};

            float sum[3] = {0.0f, 0.0f, 0.0f};
            for (int i = 0; i < kSampleCount; ++i) {
                const float xi1 = (static_cast<float>(i) + 0.5f) /
                                  static_cast<float>(kSampleCount);
                const float xi2 = radicalInverseVdC(static_cast<uint32_t>(i));

                const float r = std::sqrt(xi1);
                const float samplePhi = 2.0f * kPi * xi2;
                const float localX = r * std::cos(samplePhi);
                const float localY = r * std::sin(samplePhi);
                const float localZ = std::sqrt(std::max(0.0f, 1.0f - xi1));

                const std::array<float, 3> dir = {
                    right[0] * localX + tangent[0] * localY + N[0] * localZ,
                    right[1] * localX + tangent[1] * localY + N[1] * localZ,
                    right[2] * localX + tangent[2] * localY + N[2] * localZ};

                const auto color =
                    sampleBilinear(base, width, height, dir);
                for (int c = 0; c < 3; ++c) {
                    sum[c] += color[c];
                }
            }

            const size_t dst = (static_cast<size_t>(y) * irrWidth + x) * 4;
            for (int c = 0; c < 3; ++c) {
                irradiance[dst + c] = kPi * sum[c] /
                                      static_cast<float>(kSampleCount);
            }
            irradiance[dst + 3] = 1.0f;
        }
    }

    return irradiance;
}

} // namespace

float geometrySchlickGGX(float ndotv, float roughness) {
    const float a = roughness;
    const float k = (a * a) / 2.0f;
    return ndotv / (ndotv * (1.0f - k) + k);
}

float geometrySmith(float ndotv, float ndotl, float roughness) {
    return geometrySchlickGGX(ndotv, roughness) *
           geometrySchlickGGX(ndotl, roughness);
}

std::vector<float> buildBrdfLut(uint32_t size) {
    std::vector<float> lut(static_cast<size_t>(size) * size * 4, 0.0f);
    constexpr int kSampleCount = 128;

    for (uint32_t y = 0; y < size; ++y) {
        const float roughness = (static_cast<float>(y) + 0.5f) /
                                static_cast<float>(size);
        for (uint32_t x = 0; x < size; ++x) {
            const float ndotv = (static_cast<float>(x) + 0.5f) /
                                static_cast<float>(size);
            const float sinThetaV =
                std::sqrt(std::max(0.0f, 1.0f - ndotv * ndotv));
            const std::array<float, 3> V = {sinThetaV, 0.0f, ndotv};

            float sumA = 0.0f;
            float sumB = 0.0f;

            for (int i = 0; i < kSampleCount; ++i) {
                const float xi1 = (static_cast<float>(i) + 0.5f) /
                                  static_cast<float>(kSampleCount);
                const float xi2 = radicalInverseVdC(static_cast<uint32_t>(i));

                const float a = roughness * roughness;
                const float phi = 2.0f * kPi * xi2;
                const float cosTheta = std::sqrt(
                    (1.0f - xi1) / (1.0f + (a * a - 1.0f) * xi1));
                const float sinTheta =
                    std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

                const std::array<float, 3> H = {
                    sinTheta * std::cos(phi),
                    sinTheta * std::sin(phi),
                    cosTheta};

                const float vdoth = V[0] * H[0] + V[1] * H[1] + V[2] * H[2];
                const std::array<float, 3> L = {
                    2.0f * vdoth * H[0] - V[0],
                    2.0f * vdoth * H[1] - V[1],
                    2.0f * vdoth * H[2] - V[2]};

                if (L[2] <= 0.0f) {
                    continue;
                }

                const float ndotl = std::max(L[2], 0.0f);
                const float ndoth = std::max(H[2], 0.0f);
                const float vdh = std::max(vdoth, 0.0f);

                const float G = geometrySmith(ndotv, ndotl, roughness);
                const float gVis =
                    (G * vdh) / std::max(ndoth * ndotv, 1e-4f);
                const float fc = std::pow(1.0f - vdh, 5.0f);

                sumA += (1.0f - fc) * gVis;
                sumB += fc * gVis;
            }

            const size_t dst = (static_cast<size_t>(y) * size + x) * 4;
            lut[dst + 0] = sumA / static_cast<float>(kSampleCount);
            lut[dst + 1] = sumB / static_cast<float>(kSampleCount);
            lut[dst + 2] = 0.0f;
            lut[dst + 3] = 1.0f;
        }
    }
    return lut;
}
float luminanceOf(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

std::vector<minitracer::EnvSampleEntry> buildEnvSamples(
    const minitracer::EnvironmentMap& map,
    uint32_t sampleWidth,
    uint32_t sampleHeight,
    float& pdfScale) {
    if (map.width == 0 || map.height == 0) {
        pdfScale = 1.0f;
        return {};
    }

    const uint32_t sw = std::max(1u, std::min(sampleWidth, map.width));
    const uint32_t sh = std::max(1u, std::min(sampleHeight, map.height));
    const uint32_t count = sw * sh;

    std::vector<float> weights(count, 0.0f);
    std::vector<float> luminances(count, 0.0f);

    for (uint32_t y = 0; y < sh; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) /
                        static_cast<float>(sh);
        const float theta = v * kPi;
        const float sinTheta = std::sin(theta);

        const uint32_t y0 = y * map.height / sh;
        const uint32_t y1 = std::max(y0 + 1, (y + 1) * map.height / sh);

        for (uint32_t x = 0; x < sw; ++x) {
            const uint32_t x0 = x * map.width / sw;
            const uint32_t x1 = std::max(x0 + 1, (x + 1) * map.width / sw);

            float sum[3] = {0.0f, 0.0f, 0.0f};
            int samples = 0;
            for (uint32_t sy = y0; sy < y1; ++sy) {
                for (uint32_t sx = x0; sx < x1; ++sx) {
                    const size_t src =
                        (static_cast<size_t>(sy) * map.width + sx) * 4;
                    sum[0] += map.pixels[src + 0];
                    sum[1] += map.pixels[src + 1];
                    sum[2] += map.pixels[src + 2];
                    ++samples;
                }
            }
            const float inv = 1.0f / static_cast<float>(std::max(samples, 1));
            const float lum = luminanceOf(
                sum[0] * inv, sum[1] * inv, sum[2] * inv);
            const uint32_t index = y * sw + x;
            luminances[index] = lum;
            weights[index] = lum * sinTheta;
        }
    }

    float totalWeight = 0.0f;
    for (float weight : weights) {
        totalWeight += weight;
    }
    if (totalWeight <= 0.0f) {
        pdfScale = 1.0f;
        return {};
    }

    std::vector<float> scaled(count, 0.0f);
    std::vector<float> probability(count, 0.0f);
    std::vector<uint32_t> alias(count, 0);
    std::vector<int> small;
    std::vector<int> large;

    for (uint32_t i = 0; i < count; ++i) {
        scaled[i] = weights[i] * static_cast<float>(count) / totalWeight;
        alias[i] = i;
        if (scaled[i] < 1.0f) {
            small.push_back(static_cast<int>(i));
        } else {
            large.push_back(static_cast<int>(i));
        }
    }

    while (!small.empty() && !large.empty()) {
        const int l = small.back();
        small.pop_back();
        const int g = large.back();
        large.pop_back();

        probability[l] = scaled[l];
        alias[l] = static_cast<uint32_t>(g);
        scaled[g] = scaled[g] + scaled[l] - 1.0f;

        if (scaled[g] < 1.0f) {
            small.push_back(g);
        } else {
            large.push_back(g);
        }
    }

    while (!large.empty()) {
        probability[large.back()] = 1.0f;
        large.pop_back();
    }
    while (!small.empty()) {
        probability[small.back()] = 1.0f;
        small.pop_back();
    }

    const float du = 1.0f / static_cast<float>(sw);
    const float dv = 1.0f / static_cast<float>(sh);
    pdfScale = 1.0f /
               (totalWeight * 2.0f * kPi * kPi * du * dv);

    std::vector<minitracer::EnvSampleEntry> samples(count);
    for (uint32_t i = 0; i < count; ++i) {
        samples[i].probability = probability[i];
        samples[i].alias = alias[i];
        samples[i].x = i % sw;
        samples[i].y = i / sw;
        samples[i].pdf = luminances[i] * pdfScale;
    }
    return samples;
}
namespace minitracer {

EnvironmentMap loadEnvironmentMap(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(0);
    float* data = stbi_loadf(path.u8string().c_str(), &width, &height, &channels, 4);
    if (!data || width <= 0 || height <= 0) {
        throw std::runtime_error(
            "Failed to load environment map: " + path.string());
    }

    const uint32_t targetWidth =
        static_cast<uint32_t>(std::min(width, 2048));
    const uint32_t targetHeight = static_cast<uint32_t>(
        std::max(1, height * static_cast<int>(targetWidth) / width));

    EnvironmentMap map;
    map.width = targetWidth;
    map.height = targetHeight;
    map.pixels.resize(static_cast<size_t>(targetWidth) * targetHeight * 4);

    double average[3] = {0.0, 0.0, 0.0};
    for (uint32_t y = 0; y < targetHeight; ++y) {
        const uint32_t sourceY = y * static_cast<uint32_t>(height) / targetHeight;
        for (uint32_t x = 0; x < targetWidth; ++x) {
            const uint32_t sourceX = x * static_cast<uint32_t>(width) / targetWidth;
            const float* source = data + (static_cast<size_t>(sourceY) * width + sourceX) * 4;
            const size_t dest = (static_cast<size_t>(y) * targetWidth + x) * 4;

            map.pixels[dest + 0] = source[0];
            map.pixels[dest + 1] = source[1];
            map.pixels[dest + 2] = source[2];
            map.pixels[dest + 3] = 1.0f;

            average[0] += source[0];
            average[1] += source[1];
            average[2] += source[2];
        }
    }

    const double pixelCount =
        static_cast<double>(targetWidth) * static_cast<double>(targetHeight);
    map.averageColor[0] = static_cast<float>(average[0] / pixelCount);
    map.averageColor[1] = static_cast<float>(average[1] / pixelCount);
    map.averageColor[2] = static_cast<float>(average[2] / pixelCount);

    stbi_image_free(data);

    map.mips = buildMips(map.pixels, map.width, map.height);

    map.irradianceWidth = 32;
    map.irradianceHeight = 16;
    map.irradiance = buildIrradiance(
        map.pixels,
        map.width,
        map.height,
        map.irradianceWidth,
        map.irradianceHeight);

    map.brdfLutWidth = 256;
    map.brdfLutHeight = 256;
    map.brdfLut = buildBrdfLut(map.brdfLutWidth);

    map.sampleWidth = std::min(map.width, 128u);
    map.sampleHeight = std::min(map.height, 64u);
    map.envSamples = buildEnvSamples(
        map, map.sampleWidth, map.sampleHeight, map.environmentPdfScale);

    return map;
}

} // namespace minitracer