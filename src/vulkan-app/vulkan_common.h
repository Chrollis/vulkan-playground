#pragma once

#include "vulkan_app.h"

#include "bvh_builder.h"
#include "env_map.h"
#include "gltf_loader.h"
#include "obj_loader.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

inline void logMessage(const std::string& message) {
    std::cout << message << std::endl;
    OutputDebugStringA((message + "\n").c_str());
    std::ofstream file("Vulkan-Playground.log", std::ios::app);
    if (file.is_open()) {
        file << message << '\n';
    }
}

inline std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    const auto size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

inline bool loadImageRGBA(
    const std::filesystem::path& path,
    uint32_t& width,
    uint32_t& height,
    std::vector<uint8_t>& pixels) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)))) {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(
            path.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &decoder))) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        return false;
    }

    if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0) {
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

    pixels.resize(static_cast<size_t>(width) * height * 4);
    const UINT stride = width * 4;
    const UINT bufferSize = static_cast<UINT>(pixels.size());
    return SUCCEEDED(converter->CopyPixels(
        nullptr, stride, bufferSize, pixels.data()));
}

inline minitracer::Vec3 vec3Sub(
    const minitracer::Vec3& a, const minitracer::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline minitracer::Vec3 vec3Cross(
    const minitracer::Vec3& a, const minitracer::Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

inline float vec3Length(const minitracer::Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline minitracer::Vec3 vec3Normalize(const minitracer::Vec3& v) {
    const float len = vec3Length(v);
    if (len <= 1e-8f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}