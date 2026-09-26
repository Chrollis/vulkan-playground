#pragma once

#include <filesystem>

#include "mesh_types.h"

namespace minitracer {

Mesh loadGltf(const std::filesystem::path& path);

} // namespace minitracer
