#include "vulkan_common.h"

void VulkanApp::createHud() {
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = HUD_WIDTH;
    bitmapInfo.bmiHeader.biHeight = -HUD_HEIGHT;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    hudDC_ = CreateCompatibleDC(screenDC);
    hudBitmap_ = CreateDIBSection(
        hudDC_, &bitmapInfo, DIB_RGB_COLORS, &hudBits_, nullptr, 0);
    ReleaseDC(nullptr, screenDC);
    SelectObject(hudDC_, hudBitmap_);

    hudFont_ = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FF_DONTCARE, L"Consolas");
    SelectObject(hudDC_, hudFont_);
    SetBkMode(hudDC_, TRANSPARENT);
    SetTextColor(hudDC_, RGB(255, 255, 255));

    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = vk::Format::eR8G8B8A8Unorm;
    imageInfo.extent = vk::Extent3D(HUD_WIDTH, HUD_HEIGHT, 1);
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    hudImage_ = device_.createImage(imageInfo);

    auto requirements = hudImage_.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{};
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    hudImageMemory_ = device_.allocateMemory(allocInfo);
    hudImage_.bindMemory(*hudImageMemory_, 0);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = *hudImage_;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eR8G8B8A8Unorm;
    viewInfo.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    hudImageView_ = device_.createImageView(viewInfo);

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = vk::False;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = vk::False;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = vk::False;
    hudSampler_ = device_.createSampler(samplerInfo);

    const vk::DeviceSize hudBytes =
        static_cast<vk::DeviceSize>(HUD_WIDTH) * HUD_HEIGHT * 4;
    vk::BufferCreateInfo stagingInfo{};
    stagingInfo.size = hudBytes;
    stagingInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    stagingInfo.sharingMode = vk::SharingMode::eExclusive;
    hudStagingBuffer_ = device_.createBuffer(stagingInfo);
    auto stagingReq = hudStagingBuffer_.getMemoryRequirements();

    vk::MemoryAllocateInfo stagingAlloc{};
    stagingAlloc.allocationSize = stagingReq.size;
    stagingAlloc.memoryTypeIndex = findMemoryType(
        stagingReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    hudStagingMemory_ = device_.allocateMemory(stagingAlloc);
    hudStagingBuffer_.bindMemory(*hudStagingMemory_, 0);
    hudMapped_ = hudStagingMemory_.mapMemory(0, hudBytes);

    updateHud();
}

void VulkanApp::updateHud() {
    if (!hudBits_ || !hudMapped_) {
        return;
    }

    std::memset(hudBits_, 0, static_cast<size_t>(HUD_WIDTH) * HUD_HEIGHT * 4);

    SelectObject(hudDC_, hudFont_);
    SetBkMode(hudDC_, TRANSPARENT);
    SetTextColor(hudDC_, RGB(255, 255, 255));

    int y = 8;
    auto draw = [&](int x, const char* text) {
        TextOutA(hudDC_, x, y, text, static_cast<int>(std::strlen(text)));
    };

    draw(8, "Camera");
    y += 20;
    draw(16, "Orbit");
    y += 18;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "Dist %.2f",
                  static_cast<double>(cameraDistance_));
    draw(16, buffer);
    y += 26;

    draw(8, "Environment");
    y += 20;
    std::snprintf(buffer, sizeof(buffer), "Exposure %.2f",
                  static_cast<double>(exposure_));
    draw(16, buffer);
    y += 18;
    std::snprintf(buffer, sizeof(buffer), "Rot %.2f",
                  static_cast<double>(environmentRotation_));
    draw(16, buffer);
    y += 18;
    std::snprintf(buffer, sizeof(buffer), "Intensity %.2f",
                  static_cast<double>(environmentIntensity_));
    draw(16, buffer);

    std::memcpy(
        hudMapped_, hudBits_,
        static_cast<size_t>(HUD_WIDTH) * HUD_HEIGHT * 4);
}
