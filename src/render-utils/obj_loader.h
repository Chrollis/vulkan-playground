#pragma once

#include <filesystem>

#include "mesh_types.h"

namespace minitracer {

Mesh loadObj(const std::filesystem::path& path);

} // namespace minitracer
