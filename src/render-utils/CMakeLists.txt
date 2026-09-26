cmake_minimum_required(VERSION 3.21)

# Asset side: glTF / OBJ loading, texture decoding, BVH and HDR environment
# precomputation. No Vulkan dependency.
add_library(render-utils STATIC
    gltf_loader.cpp
    obj_loader.cpp
    bvh_builder.cpp
    env_map.cpp
)

target_include_directories(render-utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cgltf
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/stb
)

target_link_libraries(render-utils PUBLIC
    windowscodecs
)

target_compile_definitions(render-utils PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)
