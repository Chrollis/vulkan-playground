#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "mesh_types.h"

class VulkanApp {
public:
    explicit VulkanApp(HWND hwnd);
    void init();
    void render();
    void cleanup();

    void onLeftMouseDown();
    void onLeftMouseUp();
    void onRightMouseDown();
    void onRightMouseUp();
    void onMouseMove(int x, int y);
    void onMouseWheel(int wheelDelta);
    void onKeyDown(unsigned int key);
    void onKeyUp(unsigned int key);
    void resetCamera();

private:
    struct QueueFamilyIndices {
        uint32_t graphics = UINT32_MAX;
        uint32_t present = UINT32_MAX;
        bool complete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
    };

    struct SwapchainSupport {
        vk::SurfaceCapabilitiesKHR caps;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> modes;
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* userData);

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void printDeviceInfo();
    void updateWindowTitle();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createDescriptorSetLayouts();
    void createGraphicsPipeline();
    void createComputePipeline();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createRenderFinishedSemaphores();
    void createComputeTarget();
    void createSampler();
    void createDescriptorPool();
    void allocateDescriptorSets();
    void updateDescriptorSets();
    void createMeshBuffers();
    void createDeviceLocalBuffer(
        const void* data,
        vk::DeviceSize size,
        vk::raii::Buffer& buffer,
        vk::raii::DeviceMemory& memory);
    void createTexture();
    void createTextureArray(
        const std::vector<minitracer::TextureData>& layers,
        vk::Format format,
        const char* debugName,
        vk::raii::Image& image,
        vk::raii::DeviceMemory& memory,
        vk::raii::ImageView& view,
        uint32_t& mipLevels);
    void ensureFallbackTexture();
    void createEnvironment();
    void createHud();
    void updateHud();
    void recreateSwapchain();
    void destroySwapchain();
    void destroyRenderFinishedSemaphores();
    void destroyComputeTarget();
    void destroySyncObjects();

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) const;
    SwapchainSupport querySwapchainSupport(vk::PhysicalDevice device) const;
    vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const;
    vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& modes) const;
    vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& caps) const;
    uint32_t findMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties) const;
    vk::raii::ShaderModule loadShader(const char* filename) const;
    bool validationLayerAvailable() const;

    HWND hwnd_ = nullptr;
    minitracer::Mesh mesh_;

    vk::raii::Context context_;
    vk::raii::Instance instance_{ nullptr };
    vk::raii::DebugUtilsMessengerEXT debugMessenger_{ nullptr };
    vk::raii::SurfaceKHR surface_{ nullptr };
    vk::raii::PhysicalDevice physicalDevice_{ nullptr };
    vk::raii::Device device_{ nullptr };
    vk::raii::Queue graphicsQueue_{ nullptr };
    vk::raii::Queue presentQueue_{ nullptr };

    vk::raii::SwapchainKHR swapchain_{ nullptr };
    std::vector<vk::Image> swapchainImages_;
    std::vector<vk::raii::ImageView> swapchainImageViews_;
    vk::Format swapchainFormat_{};
    vk::Extent2D swapchainExtent_{};

    vk::raii::DescriptorSetLayout computeSetLayout_{ nullptr };
    vk::raii::DescriptorSetLayout graphicsSetLayout_{ nullptr };
    vk::raii::PipelineLayout computePipelineLayout_{ nullptr };
    vk::raii::PipelineLayout graphicsPipelineLayout_{ nullptr };
    vk::raii::Pipeline computePipeline_{ nullptr };
    vk::raii::Pipeline graphicsPipeline_{ nullptr };

    vk::raii::DeviceMemory accumImageMemory_{ nullptr };
    vk::raii::Image accumImage_{ nullptr };
    vk::raii::ImageView accumImageView_{ nullptr };
    vk::raii::DeviceMemory displayImageMemory_{ nullptr };
    vk::raii::Image displayImage_{ nullptr };
    vk::raii::ImageView displayImageView_{ nullptr };
    uint32_t frameIndex_ = 0;
    vk::raii::Sampler computeSampler_{ nullptr };

    vk::raii::DeviceMemory textureSrgbImageMemory_{ nullptr };
    vk::raii::Image textureSrgbImage_{ nullptr };
    vk::raii::ImageView textureSrgbImageView_{ nullptr };
    uint32_t textureSrgbMipLevels_ = 1;

    vk::raii::DeviceMemory textureLinearImageMemory_{ nullptr };
    vk::raii::Image textureLinearImage_{ nullptr };
    vk::raii::ImageView textureLinearImageView_{ nullptr };
    uint32_t textureLinearMipLevels_ = 1;

    vk::raii::DeviceMemory texSlotBufferMemory_{ nullptr };
    vk::raii::Buffer texSlotBuffer_{ nullptr };
    vk::raii::DeviceMemory texTransformBufferMemory_{ nullptr };
    vk::raii::Buffer texTransformBuffer_{ nullptr };

    vk::raii::DeviceMemory envImageMemory_{ nullptr };
    vk::raii::Image envImage_{ nullptr };
    vk::raii::ImageView envImageView_{ nullptr };
    vk::raii::Sampler envSampler_{ nullptr };

    vk::raii::DeviceMemory irradianceImageMemory_{ nullptr };
    vk::raii::Image irradianceImage_{ nullptr };
    vk::raii::ImageView irradianceImageView_{ nullptr };
    vk::raii::Sampler irradianceSampler_{ nullptr };

    vk::raii::DeviceMemory brdfLutImageMemory_{ nullptr };
    vk::raii::Image brdfLutImage_{ nullptr };
    vk::raii::ImageView brdfLutImageView_{ nullptr };
    vk::raii::Sampler brdfLutSampler_{ nullptr };

    vk::raii::DeviceMemory envSampleBufferMemory_{ nullptr };
    vk::raii::Buffer envSampleBuffer_{ nullptr };

    static constexpr int HUD_WIDTH = 256;
    static constexpr int HUD_HEIGHT = 160;
    HDC hudDC_ = nullptr;
    HBITMAP hudBitmap_ = nullptr;
    HFONT hudFont_ = nullptr;
    void* hudBits_ = nullptr;
    void* hudMapped_ = nullptr;
    vk::raii::DeviceMemory hudImageMemory_{ nullptr };
    vk::raii::Image hudImage_{ nullptr };
    vk::raii::ImageView hudImageView_{ nullptr };
    vk::raii::Sampler hudSampler_{ nullptr };
    vk::raii::Buffer hudStagingBuffer_{ nullptr };
    vk::raii::DeviceMemory hudStagingMemory_{ nullptr };
    bool showHud_ = true;

    vk::raii::DeviceMemory vertexBufferMemory_{ nullptr };
    vk::raii::Buffer vertexBuffer_{ nullptr };
    vk::raii::DeviceMemory triangleBufferMemory_{ nullptr };
    vk::raii::Buffer triangleBuffer_{ nullptr };
    vk::raii::DeviceMemory materialBufferMemory_{ nullptr };
    vk::raii::Buffer materialBuffer_{ nullptr };
    vk::raii::DeviceMemory bvhBufferMemory_{ nullptr };
    vk::raii::Buffer bvhBuffer_{ nullptr };
    vk::raii::DeviceMemory lightBufferMemory_{ nullptr };
    vk::raii::Buffer lightBuffer_{ nullptr };

    vk::raii::DescriptorPool descriptorPool_{ nullptr };
    std::vector<vk::raii::DescriptorSet> descriptorSets_;

    vk::raii::CommandPool commandPool_{ nullptr };
    std::vector<vk::raii::CommandBuffer> commandBuffers_;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence> inFlightFences_;
    uint32_t currentFrame_ = 0;

    bool validationEnabled_ = false;
    std::wstring windowTitle_;
    std::wstring gpuName_;

    float exposure_ = 1.0f;
    float environmentRotation_ = 0.0f;
    float environmentIntensity_ = 0.8f;
    float environmentPdfScale_ = 1.0f;
    int32_t environmentSampleWidth_ = 0;
    int32_t environmentSampleHeight_ = 0;
    bool showLightGizmos_ = true;

    bool keyDown_[256] = {};

    bool leftDragging_ = false;
    bool rightDragging_ = false;
    int lastMouseX_ = 0;
    int lastMouseY_ = 0;

    minitracer::Vec3 cameraTarget_{0.0f, 0.0f, 0.0f};
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.2f;
    float cameraDistance_ = 2.5f;

    minitracer::Vec3 cameraPosition_{0.0f, 0.0f, 2.5f};
    minitracer::Vec3 cameraForward_{0.0f, 0.0f, -1.0f};
    minitracer::Vec3 cameraRight_{1.0f, 0.0f, 0.0f};
    minitracer::Vec3 cameraUp_{0.0f, 1.0f, 0.0f};

    void updateCamera(float deltaTime);
    void updateCameraBasis();
};

