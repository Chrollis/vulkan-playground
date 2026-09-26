#include "vulkan_common.h"

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanApp::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) {
    const char* level = "INFO";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        level = "WARN";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        level = "ERROR";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        level = "VERBOSE";
    }
    logMessage(std::string("[Vulkan ") + level + "] " + data->pMessage);
    return VK_FALSE;
}

VulkanApp::VulkanApp(HWND hwnd) : hwnd_(hwnd) {
    mesh_ = minitracer::make_demo_triangle();
    updateCameraBasis();
}
