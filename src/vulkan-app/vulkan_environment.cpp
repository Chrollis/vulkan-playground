#include "vulkan_common.h"

void VulkanApp::createEnvironment() {
    const std::string envPath = std::string(ASSET_DIR) + "/env.hdr";
    const minitracer::EnvironmentMap env =
        minitracer::loadEnvironmentMap(envPath);
    logMessage("Loaded HDR environment: " + envPath + " (" +
               std::to_string(env.width) + "x" + std::to_string(env.height) +
               ", mips=" + std::to_string(env.mips.size()) + ")");

    environmentPdfScale_ = env.environmentPdfScale;
    environmentSampleWidth_ = static_cast<int32_t>(env.sampleWidth);
    environmentSampleHeight_ = static_cast<int32_t>(env.sampleHeight);

    const uint32_t mipCount = static_cast<uint32_t>(env.mips.size());
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = vk::Format::eR32G32B32A32Sfloat;
    imageInfo.extent = vk::Extent3D(env.width, env.height, 1);
    imageInfo.mipLevels = mipCount;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    envImage_ = device_.createImage(imageInfo);

    auto envRequirements = envImage_.getMemoryRequirements();
    vk::MemoryAllocateInfo envAlloc{};
    envAlloc.allocationSize = envRequirements.size;
    envAlloc.memoryTypeIndex = findMemoryType(
        envRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    envImageMemory_ = device_.allocateMemory(envAlloc);
    envImage_.bindMemory(*envImageMemory_, 0);

    vk::ImageViewCreateInfo envViewInfo{};
    envViewInfo.image = *envImage_;
    envViewInfo.viewType = vk::ImageViewType::e2D;
    envViewInfo.format = vk::Format::eR32G32B32A32Sfloat;
    envViewInfo.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, mipCount, 0, 1};
    envImageView_ = device_.createImageView(envViewInfo);

    std::vector<vk::DeviceSize> mipOffsets(mipCount);
    vk::DeviceSize totalEnvBytes = 0;
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        mipOffsets[mip] = totalEnvBytes;
        totalEnvBytes +=
            static_cast<vk::DeviceSize>(env.mips[mip].size()) * sizeof(float);
    }

    vk::BufferCreateInfo envStagingInfo{};
    envStagingInfo.size = totalEnvBytes;
    envStagingInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    envStagingInfo.sharingMode = vk::SharingMode::eExclusive;
    auto envStaging = device_.createBuffer(envStagingInfo);
    auto envStagingReq = envStaging.getMemoryRequirements();

    vk::MemoryAllocateInfo envStagingAlloc{};
    envStagingAlloc.allocationSize = envStagingReq.size;
    envStagingAlloc.memoryTypeIndex = findMemoryType(
        envStagingReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    auto envStagingMemory = device_.allocateMemory(envStagingAlloc);
    envStaging.bindMemory(*envStagingMemory, 0);

    void* envMapped = envStagingMemory.mapMemory(0, totalEnvBytes);
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        std::memcpy(
            static_cast<uint8_t*>(envMapped) + mipOffsets[mip],
            env.mips[mip].data(),
            env.mips[mip].size() * sizeof(float));
    }
    envStagingMemory.unmapMemory();

    vk::CommandBufferAllocateInfo allocCmd{};
    allocCmd.commandPool = *commandPool_;
    allocCmd.level = vk::CommandBufferLevel::ePrimary;
    allocCmd.commandBufferCount = 1;
    auto cmds = device_.allocateCommandBuffers(allocCmd);
    auto cmd = std::move(cmds[0]);
    cmd.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    vk::ImageMemoryBarrier2 toTransfer{};
    toTransfer.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    toTransfer.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    toTransfer.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
    toTransfer.oldLayout = vk::ImageLayout::eUndefined;
    toTransfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
    toTransfer.image = *envImage_;
    toTransfer.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, mipCount, 0, 1};
    vk::DependencyInfo dependency{};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &toTransfer;
    cmd.pipelineBarrier2(dependency);

    uint32_t mipWidth = env.width;
    uint32_t mipHeight = env.height;
    std::vector<vk::BufferImageCopy> copies(mipCount);
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        copies[mip].bufferOffset = mipOffsets[mip];
        copies[mip].imageSubresource = vk::ImageSubresourceLayers{
            vk::ImageAspectFlagBits::eColor, mip, 0, 1};
        copies[mip].imageExtent = vk::Extent3D(mipWidth, mipHeight, 1);
        mipWidth = std::max(1u, mipWidth / 2u);
        mipHeight = std::max(1u, mipHeight / 2u);
    }
    cmd.copyBufferToImage(
        *envStaging,
        *envImage_,
        vk::ImageLayout::eTransferDstOptimal,
        copies);

    vk::ImageMemoryBarrier2 toShader{};
    toShader.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    toShader.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    toShader.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    toShader.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    toShader.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    toShader.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    toShader.image = *envImage_;
    toShader.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, mipCount, 0, 1};
    dependency.pImageMemoryBarriers = &toShader;
    cmd.pipelineBarrier2(dependency);
    cmd.end();

    vk::CommandBuffer rawEnvCmd = *cmd;
    vk::SubmitInfo envSubmit{};
    envSubmit.commandBufferCount = 1;
    envSubmit.pCommandBuffers = &rawEnvCmd;
    (void)graphicsQueue_.submit(envSubmit, nullptr);
    (void)graphicsQueue_.waitIdle();

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = vk::False;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = vk::False;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = vk::False;
    samplerInfo.maxLod = static_cast<float>(mipCount - 1);
    envSampler_ = device_.createSampler(samplerInfo);

    const vk::DeviceSize irradianceBytes =
        static_cast<vk::DeviceSize>(env.irradiance.size()) * sizeof(float);

    vk::ImageCreateInfo irradianceInfo{};
    irradianceInfo.imageType = vk::ImageType::e2D;
    irradianceInfo.format = vk::Format::eR32G32B32A32Sfloat;
    irradianceInfo.extent = vk::Extent3D(env.irradianceWidth, env.irradianceHeight, 1);
    irradianceInfo.mipLevels = 1;
    irradianceInfo.arrayLayers = 1;
    irradianceInfo.samples = vk::SampleCountFlagBits::e1;
    irradianceInfo.tiling = vk::ImageTiling::eOptimal;
    irradianceInfo.usage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    irradianceInfo.sharingMode = vk::SharingMode::eExclusive;
    irradianceInfo.initialLayout = vk::ImageLayout::eUndefined;
    irradianceImage_ = device_.createImage(irradianceInfo);

    auto irradianceReq = irradianceImage_.getMemoryRequirements();
    vk::MemoryAllocateInfo irradianceAlloc{};
    irradianceAlloc.allocationSize = irradianceReq.size;
    irradianceAlloc.memoryTypeIndex = findMemoryType(
        irradianceReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    irradianceImageMemory_ = device_.allocateMemory(irradianceAlloc);
    irradianceImage_.bindMemory(*irradianceImageMemory_, 0);

    vk::ImageViewCreateInfo irradianceViewInfo{};
    irradianceViewInfo.image = *irradianceImage_;
    irradianceViewInfo.viewType = vk::ImageViewType::e2D;
    irradianceViewInfo.format = vk::Format::eR32G32B32A32Sfloat;
    irradianceViewInfo.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    irradianceImageView_ = device_.createImageView(irradianceViewInfo);

    vk::BufferCreateInfo irrStagingInfo{};
    irrStagingInfo.size = irradianceBytes;
    irrStagingInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    irrStagingInfo.sharingMode = vk::SharingMode::eExclusive;
    auto irrStaging = device_.createBuffer(irrStagingInfo);
    auto irrStagingReq = irrStaging.getMemoryRequirements();

    vk::MemoryAllocateInfo irrStagingAlloc{};
    irrStagingAlloc.allocationSize = irrStagingReq.size;
    irrStagingAlloc.memoryTypeIndex = findMemoryType(
        irrStagingReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    auto irrStagingMemory = device_.allocateMemory(irrStagingAlloc);
    irrStaging.bindMemory(*irrStagingMemory, 0);

    void* irrMapped = irrStagingMemory.mapMemory(0, irradianceBytes);
    std::memcpy(irrMapped, env.irradiance.data(), irradianceBytes);
    irrStagingMemory.unmapMemory();

    auto irrCmds = device_.allocateCommandBuffers(allocCmd);
    auto irrCmd = std::move(irrCmds[0]);
    irrCmd.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    vk::ImageMemoryBarrier2 irrToTransfer{};
    irrToTransfer.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    irrToTransfer.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    irrToTransfer.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
    irrToTransfer.oldLayout = vk::ImageLayout::eUndefined;
    irrToTransfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
    irrToTransfer.image = *irradianceImage_;
    irrToTransfer.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    vk::DependencyInfo irrDependency{};
    irrDependency.imageMemoryBarrierCount = 1;
    irrDependency.pImageMemoryBarriers = &irrToTransfer;
    irrCmd.pipelineBarrier2(irrDependency);

    vk::BufferImageCopy irrCopy{};
    irrCopy.imageSubresource = vk::ImageSubresourceLayers{
        vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    irrCopy.imageExtent = vk::Extent3D(env.irradianceWidth, env.irradianceHeight, 1);
    irrCmd.copyBufferToImage(
        *irrStaging,
        *irradianceImage_,
        vk::ImageLayout::eTransferDstOptimal,
        irrCopy);

    vk::ImageMemoryBarrier2 irrToShader{};
    irrToShader.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    irrToShader.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    irrToShader.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    irrToShader.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    irrToShader.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    irrToShader.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    irrToShader.image = *irradianceImage_;
    irrToShader.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    irrDependency.pImageMemoryBarriers = &irrToShader;
    irrCmd.pipelineBarrier2(irrDependency);
    irrCmd.end();

    vk::CommandBuffer rawIrrCmd = *irrCmd;
    vk::SubmitInfo irrSubmit{};
    irrSubmit.commandBufferCount = 1;
    irrSubmit.pCommandBuffers = &rawIrrCmd;
    (void)graphicsQueue_.submit(irrSubmit, nullptr);
    (void)graphicsQueue_.waitIdle();

    vk::SamplerCreateInfo irrSamplerInfo = samplerInfo;
    irrSamplerInfo.maxLod = 1.0f;
    irradianceSampler_ = device_.createSampler(irrSamplerInfo);

    const vk::DeviceSize brdfLutBytes =
        static_cast<vk::DeviceSize>(env.brdfLut.size()) * sizeof(float);

    vk::ImageCreateInfo brdfInfo{};
    brdfInfo.imageType = vk::ImageType::e2D;
    brdfInfo.format = vk::Format::eR32G32B32A32Sfloat;
    brdfInfo.extent = vk::Extent3D(env.brdfLutWidth, env.brdfLutHeight, 1);
    brdfInfo.mipLevels = 1;
    brdfInfo.arrayLayers = 1;
    brdfInfo.samples = vk::SampleCountFlagBits::e1;
    brdfInfo.tiling = vk::ImageTiling::eOptimal;
    brdfInfo.usage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    brdfInfo.sharingMode = vk::SharingMode::eExclusive;
    brdfInfo.initialLayout = vk::ImageLayout::eUndefined;
    brdfLutImage_ = device_.createImage(brdfInfo);

    auto brdfReq = brdfLutImage_.getMemoryRequirements();
    vk::MemoryAllocateInfo brdfAlloc{};
    brdfAlloc.allocationSize = brdfReq.size;
    brdfAlloc.memoryTypeIndex = findMemoryType(
        brdfReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    brdfLutImageMemory_ = device_.allocateMemory(brdfAlloc);
    brdfLutImage_.bindMemory(*brdfLutImageMemory_, 0);

    vk::ImageViewCreateInfo brdfViewInfo{};
    brdfViewInfo.image = *brdfLutImage_;
    brdfViewInfo.viewType = vk::ImageViewType::e2D;
    brdfViewInfo.format = vk::Format::eR32G32B32A32Sfloat;
    brdfViewInfo.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    brdfLutImageView_ = device_.createImageView(brdfViewInfo);

    vk::BufferCreateInfo brdfStagingInfo{};
    brdfStagingInfo.size = brdfLutBytes;
    brdfStagingInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    brdfStagingInfo.sharingMode = vk::SharingMode::eExclusive;
    auto brdfStaging = device_.createBuffer(brdfStagingInfo);
    auto brdfStagingReq = brdfStaging.getMemoryRequirements();

    vk::MemoryAllocateInfo brdfStagingAlloc{};
    brdfStagingAlloc.allocationSize = brdfStagingReq.size;
    brdfStagingAlloc.memoryTypeIndex = findMemoryType(
        brdfStagingReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    auto brdfStagingMemory = device_.allocateMemory(brdfStagingAlloc);
    brdfStaging.bindMemory(*brdfStagingMemory, 0);

    void* brdfMapped = brdfStagingMemory.mapMemory(0, brdfLutBytes);
    std::memcpy(brdfMapped, env.brdfLut.data(), brdfLutBytes);
    brdfStagingMemory.unmapMemory();

    auto brdfCmds = device_.allocateCommandBuffers(allocCmd);
    auto brdfCmd = std::move(brdfCmds[0]);
    brdfCmd.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    vk::ImageMemoryBarrier2 brdfToTransfer{};
    brdfToTransfer.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    brdfToTransfer.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    brdfToTransfer.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
    brdfToTransfer.oldLayout = vk::ImageLayout::eUndefined;
    brdfToTransfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
    brdfToTransfer.image = *brdfLutImage_;
    brdfToTransfer.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    vk::DependencyInfo brdfDependency{};
    brdfDependency.imageMemoryBarrierCount = 1;
    brdfDependency.pImageMemoryBarriers = &brdfToTransfer;
    brdfCmd.pipelineBarrier2(brdfDependency);

    vk::BufferImageCopy brdfCopy{};
    brdfCopy.imageSubresource = vk::ImageSubresourceLayers{
        vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    brdfCopy.imageExtent = vk::Extent3D(env.brdfLutWidth, env.brdfLutHeight, 1);
    brdfCmd.copyBufferToImage(
        *brdfStaging,
        *brdfLutImage_,
        vk::ImageLayout::eTransferDstOptimal,
        brdfCopy);

    vk::ImageMemoryBarrier2 brdfToShader{};
    brdfToShader.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    brdfToShader.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    brdfToShader.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    brdfToShader.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    brdfToShader.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    brdfToShader.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    brdfToShader.image = *brdfLutImage_;
    brdfToShader.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    brdfDependency.pImageMemoryBarriers = &brdfToShader;
    brdfCmd.pipelineBarrier2(brdfDependency);
    brdfCmd.end();

    vk::CommandBuffer rawBrdfCmd = *brdfCmd;
    vk::SubmitInfo brdfSubmit{};
    brdfSubmit.commandBufferCount = 1;
    brdfSubmit.pCommandBuffers = &rawBrdfCmd;
    (void)graphicsQueue_.submit(brdfSubmit, nullptr);
    (void)graphicsQueue_.waitIdle();

    vk::SamplerCreateInfo brdfSamplerInfo = samplerInfo;
    brdfSamplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    brdfSamplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    brdfSamplerInfo.maxLod = 1.0f;
    brdfLutSampler_ = device_.createSampler(brdfSamplerInfo);

    const vk::DeviceSize sampleBytes =
        static_cast<vk::DeviceSize>(env.envSamples.size()) *
        sizeof(minitracer::EnvSampleEntry);

    vk::BufferCreateInfo sampleInfo{};
    sampleInfo.size = sampleBytes;
    sampleInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    sampleInfo.sharingMode = vk::SharingMode::eExclusive;
    envSampleBuffer_ = device_.createBuffer(sampleInfo);

    auto sampleRequirements = envSampleBuffer_.getMemoryRequirements();
    vk::MemoryAllocateInfo sampleAlloc{};
    sampleAlloc.allocationSize = sampleRequirements.size;
    sampleAlloc.memoryTypeIndex = findMemoryType(
        sampleRequirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    envSampleBufferMemory_ = device_.allocateMemory(sampleAlloc);
    envSampleBuffer_.bindMemory(*envSampleBufferMemory_, 0);

    void* sampleMapped = envSampleBufferMemory_.mapMemory(0, sampleBytes);
    std::memcpy(sampleMapped, env.envSamples.data(), sampleBytes);
    envSampleBufferMemory_.unmapMemory();
}
