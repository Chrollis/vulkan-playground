cmake_minimum_required(VERSION 3.21)

# Vulkan side: Win32 window plumbing, swapchain, pipelines and the renderer.
add_library(vulkan-app STATIC
    vulkan_app.cpp
    vulkan_camera.cpp
    vulkan_setup.cpp
    vulkan_resources.cpp
    vulkan_texture.cpp
    vulkan_environment.cpp
    vulkan_hud.cpp
    vulkan_sync.cpp
    vulkan_render.cpp
)

target_include_directories(vulkan-app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(vulkan-app PUBLIC
    render-utils
    Vulkan::Vulkan
)

target_compile_definitions(vulkan-app PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    VK_USE_PLATFORM_WIN32_KHR
    SHADER_DIR="${SHADER_BINARY_DIR_CMAKE}"
    ASSET_DIR="${ASSET_DIR_CMAKE}"
)
