#include "vulkan_common.h"

namespace {

minitracer::TextureData makeWhiteTexture() {
    minitracer::TextureData texture;
    texture.width = 1;
    texture.height = 1;
    texture.pixels.assign(4, 255);
    return texture;
}

// 1x1 layers keep a texture array binding valid when a model has no texture of
// that colour space at all.
void ensureArrayNotEmpty(std::vector<minitracer::TextureData>& layers) {
    if (layers.empty()) {
        layers.push_back(makeWhiteTexture());
    }
}

} // namespace

void VulkanApp::createDeviceLocalBuffer(const void* data,
                                        vk::DeviceSize size,
                                        vk::raii::Buffer& buffer,
                                        vk::raii::DeviceMemory& memory) {
    if (size == 0) {
        throw std::runtime_error("Cannot create an empty GPU buffer.");
    }

    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = size;
    bufferInfo.usage =
        vk::BufferUsageFlagBits::eStorageBuffer |
        vk::BufferUsageFlagBits::eTransferDst;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    buffer = device_.createBuffer(bufferInfo);
    auto requirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        requirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    memory = device_.allocateMemory(allocInfo);
    buffer.bindMemory(*memory, 0);

    vk::BufferCreateInfo stagingInfo{};
    stagingInfo.size = size;
    stagingInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    stagingInfo.sharingMode = vk::SharingMode::eExclusive;

    auto stagingBuffer = device_.createBuffer(stagingInfo);
    auto stagingRequirements = stagingBuffer.getMemoryRequirements();

    vk::MemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.allocationSize = stagingRequirements.size;
    stagingAllocInfo.memoryTypeIndex = findMemoryType(
        stagingRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    auto stagingMemory = device_.allocateMemory(stagingAllocInfo);
    stagingBuffer.bindMemory(*stagingMemory, 0);

    void* mapped = stagingMemory.mapMemory(0, size);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    stagingMemory.unmapMemory();

    vk::CommandBufferAllocateInfo allocCmd{};
    allocCmd.commandPool = *commandPool_;
    allocCmd.level = vk::CommandBufferLevel::ePrimary;
    allocCmd.commandBufferCount = 1;
    auto cmds = device_.allocateCommandBuffers(allocCmd);
    auto cmd = std::move(cmds[0]);

    cmd.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    vk::BufferCopy copy{};
    copy.size = size;
    cmd.copyBuffer(*stagingBuffer, *buffer, copy);
    cmd.end();

    vk::CommandBuffer rawCmd = *cmd;
    vk::SubmitInfo submit{};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &rawCmd;
    (void)graphicsQueue_.submit(submit, nullptr);
    (void)graphicsQueue_.waitIdle();
}

void VulkanApp::createTextureArray(
    const std::vector<minitracer::TextureData>& layers,
    vk::Format format,
    const char* debugName,
    vk::raii::Image& image,
    vk::raii::DeviceMemory& memory,
    vk::raii::ImageView& view,
    uint32_t& mipLevels) {
    if (layers.empty()) {
        throw std::runtime_error("Texture array needs at least one layer.");
    }

    const uint32_t width = layers[0].width;
    const uint32_t height = layers[0].height;
    const vk::DeviceSize layerSize =
        static_cast<vk::DeviceSize>(width) * height * 4;
    for (const auto& layer : layers) {
        if (layer.width != width || layer.height != height ||
            layer.pixels.size() != static_cast<size_t>(layerSize)) {
            throw std::runtime_error("Texture array layers must share one extent.");
        }
    }
    const uint32_t layerCount = static_cast<uint32_t>(layers.size());

    const auto formatProperties = physicalDevice_.getFormatProperties(format);
    const bool blitSupported =
        (formatProperties.optimalTilingFeatures &
         vk::FormatFeatureFlagBits::eBlitSrc) &&
        (formatProperties.optimalTilingFeatures &
         vk::FormatFeatureFlagBits::eBlitDst) &&
        (formatProperties.optimalTilingFeatures &
         vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
    mipLevels = blitSupported
                    ? 1u + static_cast<uint32_t>(std::floor(std::log2(
                               static_cast<float>(std::max(width, height)))))
                    : 1u;
    if (mipLevels == 1 && !blitSupported) {
        logMessage(std::string("Mipmap generation unavailable for ") + debugName +
                   " texture array.");
    }

    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = vk::Extent3D(width, height, 1);
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = layerCount;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst |
                      vk::ImageUsageFlagBits::eTransferSrc |
                      vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    image = device_.createImage(imageInfo);

    auto requirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    memory = device_.allocateMemory(allocInfo);
    image.bindMemory(*memory, 0);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = *image;
    viewInfo.viewType = vk::ImageViewType::e2DArray;
    viewInfo.format = format;
    viewInfo.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, layerCount};
    view = device_.createImageView(viewInfo);

    vk::BufferCreateInfo stagingInfo{};
    stagingInfo.size = layerSize * layerCount;
    stagingInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    stagingInfo.sharingMode = vk::SharingMode::eExclusive;
    auto stagingBuffer = device_.createBuffer(stagingInfo);
    auto stagingRequirements = stagingBuffer.getMemoryRequirements();

    vk::MemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.allocationSize = stagingRequirements.size;
    stagingAllocInfo.memoryTypeIndex = findMemoryType(
        stagingRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    auto stagingMemory = device_.allocateMemory(stagingAllocInfo);
    stagingBuffer.bindMemory(*stagingMemory, 0);

    void* mapped = stagingMemory.mapMemory(0, layerSize * layerCount);
    for (uint32_t layer = 0; layer < layerCount; ++layer) {
        std::memcpy(
            static_cast<uint8_t*>(mapped) + layerSize * layer,
            layers[layer].pixels.data(),
            static_cast<size_t>(layerSize));
    }
    stagingMemory.unmapMemory();

    vk::CommandBufferAllocateInfo allocCmd{};
    allocCmd.commandPool = *commandPool_;
    allocCmd.level = vk::CommandBufferLevel::ePrimary;
    allocCmd.commandBufferCount = 1;
    auto cmds = device_.allocateCommandBuffers(allocCmd);
    auto cmd = std::move(cmds[0]);
    cmd.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    auto barrier = [&](uint32_t baseLevel,
                       uint32_t levelCount,
                       vk::ImageLayout oldLayout,
                       vk::ImageLayout newLayout,
                       vk::PipelineStageFlags2 srcStage,
                       vk::PipelineStageFlags2 dstStage,
                       vk::AccessFlags2 srcAccess,
                       vk::AccessFlags2 dstAccess) {
        vk::ImageMemoryBarrier2 info{};
        info.srcStageMask = srcStage;
        info.dstStageMask = dstStage;
        info.srcAccessMask = srcAccess;
        info.dstAccessMask = dstAccess;
        info.oldLayout = oldLayout;
        info.newLayout = newLayout;
        info.image = *image;
        info.subresourceRange = vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor, baseLevel, levelCount, 0, layerCount};
        vk::DependencyInfo dependency{};
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &info;
        cmd.pipelineBarrier2(dependency);
    };

    barrier(
        0,
        mipLevels,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eNone,
        vk::AccessFlagBits2::eTransferWrite);

    std::vector<vk::BufferImageCopy> copies(layerCount);
    for (uint32_t layer = 0; layer < layerCount; ++layer) {
        copies[layer].bufferOffset = layerSize * layer;
        copies[layer].imageSubresource = vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor, 0, layer, 1};
        copies[layer].imageExtent = vk::Extent3D(width, height, 1);
    }
    cmd.copyBufferToImage(
        *stagingBuffer,
        *image,
        vk::ImageLayout::eTransferDstOptimal,
        copies);

    for (uint32_t level = 1; level < mipLevels; ++level) {
        barrier(
            level - 1,
            1,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::AccessFlagBits2::eTransferRead);

        vk::ImageBlit blit{};
        blit.srcSubresource = vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor, level - 1, 0, layerCount};
        blit.srcOffsets[1] = vk::Offset3D{
            static_cast<int32_t>(std::max(1u, width >> (level - 1))),
            static_cast<int32_t>(std::max(1u, height >> (level - 1))),
            1};
        blit.dstSubresource = vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor, level, 0, layerCount};
        blit.dstOffsets[1] = vk::Offset3D{
            static_cast<int32_t>(std::max(1u, width >> level)),
            static_cast<int32_t>(std::max(1u, height >> level)),
            1};
        cmd.blitImage(
            *image,
            vk::ImageLayout::eTransferSrcOptimal,
            *image,
            vk::ImageLayout::eTransferDstOptimal,
            blit,
            vk::Filter::eLinear);

        barrier(
            level - 1,
            1,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eTransferDstOptimal,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            vk::AccessFlagBits2::eTransferWrite);
    }

    barrier(
        0,
        mipLevels,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eTransferWrite,
        vk::AccessFlagBits2::eShaderRead);

    cmd.end();

    vk::CommandBuffer rawCmd = *cmd;
    vk::SubmitInfo submit{};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &rawCmd;
    (void)graphicsQueue_.submit(submit, nullptr);
    (void)graphicsQueue_.waitIdle();

    logMessage(std::string("Texture array [") + debugName + "] " +
               std::to_string(width) + "x" + std::to_string(height) +
               " layers=" + std::to_string(layerCount) +
               " mipLevels=" + std::to_string(mipLevels) +
               (format == vk::Format::eR8G8B8A8Srgb ? " (sRGB)" : " (UNORM)"));
}

// OBJ and the demo triangle have no glTF textures: fall back to a material
// texture path or to the procedural checker, both as sRGB layer 0.
void VulkanApp::ensureFallbackTexture() {
    if (!mesh_.texturesSrgb.empty() || !mesh_.texturesLinear.empty() ||
        !mesh_.texSlots.empty()) {
        return;
    }

    minitracer::TextureData texture;
    bool loaded = false;

    for (const auto& texturePath : mesh_.materialTexturePaths) {
        if (texturePath.empty()) {
            continue;
        }
        const std::filesystem::path path = std::filesystem::u8path(texturePath);
        if (loadImageRGBA(path, texture.width, texture.height, texture.pixels)) {
            loaded = true;
            logMessage("Loaded texture: " + texturePath);
            break;
        }
        logMessage("Failed to load texture: " + texturePath);
    }

    if (!loaded) {
        const uint32_t size = 256;
        const uint32_t cells = 16;
        texture.width = size;
        texture.height = size;
        texture.pixels.resize(static_cast<size_t>(size) * size * 4);
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                const bool white =
                    (((x * cells) / size) + ((y * cells) / size)) % 2 == 0;
                const uint8_t value = white ? 255 : 40;
                const size_t index = (static_cast<size_t>(y) * size + x) * 4;
                texture.pixels[index + 0] = value;
                texture.pixels[index + 1] = value;
                texture.pixels[index + 2] = value;
                texture.pixels[index + 3] = 255;
            }
        }
        logMessage("Using procedural checker texture.");
    }

    mesh_.texturesSrgb.push_back(std::move(texture));

    // OBJ materials reference slot 0 through Material::baseColorSlot.
    minitracer::TextureSlot slot;
    slot.layer = 0;
    mesh_.texSlots.push_back(slot);
}

void VulkanApp::createTexture() {
    ensureFallbackTexture();
    ensureArrayNotEmpty(mesh_.texturesSrgb);
    ensureArrayNotEmpty(mesh_.texturesLinear);

    createTextureArray(
        mesh_.texturesSrgb,
        vk::Format::eR8G8B8A8Srgb,
        "srgb",
        textureSrgbImage_,
        textureSrgbImageMemory_,
        textureSrgbImageView_,
        textureSrgbMipLevels_);
    createTextureArray(
        mesh_.texturesLinear,
        vk::Format::eR8G8B8A8Unorm,
        "linear",
        textureLinearImage_,
        textureLinearImageMemory_,
        textureLinearImageView_,
        textureLinearMipLevels_);

    // Both tables must stay non-empty so the descriptor bindings stay valid.
    if (mesh_.texSlots.empty()) {
        mesh_.texSlots.push_back(minitracer::TextureSlot{});
    }
    if (mesh_.texTransforms.empty()) {
        mesh_.texTransforms.push_back(minitracer::TextureTransform{});
    }

    createDeviceLocalBuffer(
        mesh_.texSlots.data(),
        mesh_.texSlots.size() * sizeof(minitracer::TextureSlot),
        texSlotBuffer_,
        texSlotBufferMemory_);
    createDeviceLocalBuffer(
        mesh_.texTransforms.data(),
        mesh_.texTransforms.size() * sizeof(minitracer::TextureTransform),
        texTransformBuffer_,
        texTransformBufferMemory_);

    logMessage("Texture slots=" + std::to_string(mesh_.texSlots.size()) +
               " transforms=" + std::to_string(mesh_.texTransforms.size()));
}
