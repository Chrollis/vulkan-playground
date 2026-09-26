#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace minitracer {

struct EnvSampleEntry {
    float probability = 0.0f;
    uint32_t alias = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    float pdf = 0.0f;
    float pad[3] = {0.0f, 0.0f, 0.0f};
};

struct EnvironmentMap {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<float> pixels; // RGBA float
    std::vector<std::vector<float>> mips; // RGBA float, mip chain

    uint32_t irradianceWidth = 0;
    uint32_t irradianceHeight = 0;
    std::vector<float> irradiance; // RGBA float

    uint32_t brdfLutWidth = 0;
    uint32_t brdfLutHeight = 0;
    std::vector<float> brdfLut; // RG float stored as RGBA

    float averageColor[3] = {0.0f, 0.0f, 0.0f};

    uint32_t sampleWidth = 0;
    uint32_t sampleHeight = 0;
    float environmentPdfScale = 1.0f;
    std::vector<EnvSampleEntry> envSamples;
};

EnvironmentMap loadEnvironmentMap(const std::filesystem::path& path);

} // namespace minitracer
