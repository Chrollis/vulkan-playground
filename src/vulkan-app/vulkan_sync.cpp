#include "vulkan_common.h"

void VulkanApp::createSyncObjects() {
    destroySyncObjects();

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imageAvailableSemaphores_.push_back(
            device_.createSemaphore(semaphoreInfo));
        inFlightFences_.push_back(device_.createFence(fenceInfo));
    }

    createRenderFinishedSemaphores();
}

void VulkanApp::createRenderFinishedSemaphores() {
    destroyRenderFinishedSemaphores();
    vk::SemaphoreCreateInfo info{};
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        renderFinishedSemaphores_.push_back(device_.createSemaphore(info));
    }
}

void VulkanApp::destroyRenderFinishedSemaphores() {
    renderFinishedSemaphores_.clear();
}

void VulkanApp::destroySyncObjects() {
    destroyRenderFinishedSemaphores();
    imageAvailableSemaphores_.clear();
    inFlightFences_.clear();
}

uint32_t VulkanApp::findMemoryType(
    uint32_t typeBits, vk::MemoryPropertyFlags properties) const {
    auto memoryProperties = physicalDevice_.getMemoryProperties();
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type.");
}

void VulkanApp::createComputeTarget() {
    destroyComputeTarget();

    const uint32_t width = std::max(1u, swapchainExtent_.width);
    const uint32_t height = std::max(1u, swapchainExtent_.height);

    auto createTarget = [&](vk::Format format,
                            vk::ImageUsageFlags usage,
                            vk::raii::Image& image,
                            vk::raii::DeviceMemory& memory,
                            vk::raii::ImageView& view) {
        vk::ImageCreateInfo imageInfo{};
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.format = format;
        imageInfo.extent = vk::Extent3D(width, height, 1);
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = vk::SampleCountFlagBits::e1;
        imageInfo.tiling = vk::ImageTiling::eOptimal;
        imageInfo.usage = usage;
        imageInfo.sharingMode = vk::SharingMode::eExclusive;
        imageInfo.initialLayout = vk::ImageLayout::eUndefined;
        image = device_.createImage(imageInfo);

        auto requirements = image.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            requirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        memory = device_.allocateMemory(allocInfo);
        image.bindMemory(*memory, 0);

        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image = *image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        view = device_.createImageView(viewInfo);
    };

    createTarget(
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eStorage,
        accumImage_,
        accumImageMemory_,
        accumImageView_);

    createTarget(
        vk::Format::eR8G8B8A8Unorm,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        displayImage_,
        displayImageMemory_,
        displayImageView_);

    frameIndex_ = 0;
}
void VulkanApp::destroyComputeTarget() {
    accumImageView_.clear();
    accumImage_.clear();
    accumImageMemory_.clear();
    displayImageView_.clear();
    displayImage_.clear();
    displayImageMemory_.clear();
}

