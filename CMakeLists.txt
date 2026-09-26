cmake_minimum_required(VERSION 3.21)
project(Vulkan-Playground LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This project currently targets Windows + Vulkan (Win32).")
endif()

find_package(Vulkan REQUIRED)

find_program(GLSLC_EXECUTABLE glslc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/Tools"
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK and set VULKAN_SDK.")
endif()

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_BINARY_DIR}")
file(TO_CMAKE_PATH "${SHADER_BINARY_DIR}" SHADER_BINARY_DIR_CMAKE)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets" ASSET_DIR_CMAKE)

set(VERT_SPV "${SHADER_BINARY_DIR}/triangle.vert.spv")
set(FRAG_SPV "${SHADER_BINARY_DIR}/triangle.frag.spv")
set(COMP_SPV "${SHADER_BINARY_DIR}/raytrace.comp.spv")

add_custom_command(
    OUTPUT "${VERT_SPV}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/triangle.vert" -o "${VERT_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/triangle.vert"
    COMMENT "Compiling vertex shader"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FRAG_SPV}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/triangle.frag" -o "${FRAG_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/triangle.frag"
    COMMENT "Compiling fragment shader"
    VERBATIM
)

add_custom_command(
    OUTPUT "${COMP_SPV}"
    COMMAND "${GLSLC_EXECUTABLE}" "${SHADER_SOURCE_DIR}/raytrace.comp" -o "${COMP_SPV}"
    DEPENDS "${SHADER_SOURCE_DIR}/raytrace.comp"
    COMMENT "Compiling compute shader"
    VERBATIM
)

add_custom_target(Vulkan-Playground-Shaders ALL
    DEPENDS "${VERT_SPV}" "${FRAG_SPV}" "${COMP_SPV}")

add_subdirectory(src/render-utils)
add_subdirectory(src/vulkan-app)

add_executable(Vulkan-Playground WIN32
    src/main.cpp
)

target_link_libraries(Vulkan-Playground PRIVATE
    vulkan-app
    imm32
)

target_compile_definitions(Vulkan-Playground PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    VK_USE_PLATFORM_WIN32_KHR
)

add_dependencies(Vulkan-Playground Vulkan-Playground-Shaders)