#include "vulkan_common.h"

void VulkanApp::createSampler() {
    vk::SamplerCreateInfo info{};
    info.magFilter = vk::Filter::eLinear;
    info.minFilter = vk::Filter::eLinear;
    info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    // glTF wrap modes are applied in the shader, so the sampler itself clamps.
    info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    info.anisotropyEnable = vk::False;
    info.maxAnisotropy = 1.0f;
    info.compareEnable = vk::False;
    info.compareOp = vk::CompareOp::eAlways;
    info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    info.unnormalizedCoordinates = vk::False;
    info.maxLod = static_cast<float>(
        std::max(textureSrgbMipLevels_, textureLinearMipLevels_));
    computeSampler_ = device_.createSampler(info);
}

void VulkanApp::createDescriptorPool() {
    std::array<vk::DescriptorPoolSize, 3> sizes{};
    sizes[0].type = vk::DescriptorType::eStorageImage;
    sizes[0].descriptorCount = 2;
    sizes[1].type = vk::DescriptorType::eCombinedImageSampler;
    sizes[1].descriptorCount = 7;
    sizes[2].type = vk::DescriptorType::eStorageBuffer;
    sizes[2].descriptorCount = 8;

    vk::DescriptorPoolCreateInfo info{};
    info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    info.poolSizeCount = static_cast<uint32_t>(sizes.size());
    info.pPoolSizes = sizes.data();
    info.maxSets = 2;
    descriptorPool_ = device_.createDescriptorPool(info);
}

void VulkanApp::allocateDescriptorSets() {
    std::array<vk::DescriptorSetLayout, 2> layouts = {
        *computeSetLayout_, *graphicsSetLayout_};
    vk::DescriptorSetAllocateInfo info{};
    info.descriptorPool = *descriptorPool_;
    info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    info.pSetLayouts = layouts.data();
    descriptorSets_ = device_.allocateDescriptorSets(info);
}

void VulkanApp::updateDescriptorSets() {
    vk::DescriptorImageInfo accumInfo{};
    accumInfo.imageView = *accumImageView_;
    accumInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::DescriptorImageInfo displayStorageInfo{};
    displayStorageInfo.imageView = *displayImageView_;
    displayStorageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::DescriptorImageInfo displaySampledInfo{};
    displaySampledInfo.imageView = *displayImageView_;
    displaySampledInfo.imageLayout = vk::ImageLayout::eGeneral;
    displaySampledInfo.sampler = *computeSampler_;

    vk::DescriptorBufferInfo vertexInfo{
        *vertexBuffer_, 0, vk::WholeSize};
    vk::DescriptorBufferInfo triangleInfo{
        *triangleBuffer_, 0, vk::WholeSize};
    vk::DescriptorBufferInfo materialInfo{
        *materialBuffer_, 0, vk::WholeSize};
    vk::DescriptorBufferInfo bvhInfo{
        *bvhBuffer_, 0, vk::WholeSize};
    vk::DescriptorBufferInfo lightInfo{
        *lightBuffer_, 0, vk::WholeSize};

    vk::DescriptorImageInfo textureSrgbInfo{};
    textureSrgbInfo.imageView = *textureSrgbImageView_;
    textureSrgbInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    textureSrgbInfo.sampler = *computeSampler_;

    vk::DescriptorImageInfo linearTextureInfo{};
    linearTextureInfo.imageView = *textureLinearImageView_;
    linearTextureInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    linearTextureInfo.sampler = *computeSampler_;

    vk::DescriptorImageInfo envInfo{};
    envInfo.imageView = *envImageView_;
    envInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    envInfo.sampler = *envSampler_;

    vk::DescriptorImageInfo irradianceInfo{};
    irradianceInfo.imageView = *irradianceImageView_;
    irradianceInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    irradianceInfo.sampler = *irradianceSampler_;

    vk::DescriptorImageInfo brdfInfo{};
    brdfInfo.imageView = *brdfLutImageView_;
    brdfInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    brdfInfo.sampler = *brdfLutSampler_;

    vk::DescriptorImageInfo hudInfo{};
    hudInfo.imageView = *hudImageView_;
    hudInfo.imageLayout = vk::ImageLayout::eGeneral;
    hudInfo.sampler = *hudSampler_;

    vk::DescriptorBufferInfo envSampleInfo{
        *envSampleBuffer_, 0, vk::WholeSize};

    vk::DescriptorBufferInfo texSlotInfo{
        *texSlotBuffer_, 0, vk::WholeSize};
    vk::DescriptorBufferInfo texTransformInfo{
        *texTransformBuffer_, 0, vk::WholeSize};

    std::array<vk::WriteDescriptorSet, 17> writes{};

    writes[0].dstSet = *descriptorSets_[0];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = vk::DescriptorType::eStorageImage;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &accumInfo;

    writes[1].dstSet = *descriptorSets_[0];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &vertexInfo;

    writes[2].dstSet = *descriptorSets_[0];
    writes[2].dstBinding = 2;
    writes[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &triangleInfo;

    writes[3].dstSet = *descriptorSets_[0];
    writes[3].dstBinding = 3;
    writes[3].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[3].descriptorCount = 1;
    writes[3].pBufferInfo = &materialInfo;

    writes[4].dstSet = *descriptorSets_[0];
    writes[4].dstBinding = 5;
    writes[4].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[4].descriptorCount = 1;
    writes[4].pBufferInfo = &bvhInfo;

    writes[5].dstSet = *descriptorSets_[0];
    writes[5].dstBinding = 6;
    writes[5].descriptorType = vk::DescriptorType::eStorageImage;
    writes[5].descriptorCount = 1;
    writes[5].pImageInfo = &displayStorageInfo;

    writes[6].dstSet = *descriptorSets_[0];
    writes[6].dstBinding = 7;
    writes[6].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[6].descriptorCount = 1;
    writes[6].pImageInfo = &envInfo;

    writes[7].dstSet = *descriptorSets_[0];
    writes[7].dstBinding = 8;
    writes[7].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[7].descriptorCount = 1;
    writes[7].pBufferInfo = &lightInfo;

    writes[8].dstSet = *descriptorSets_[0];
    writes[8].dstBinding = 9;
    writes[8].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[8].descriptorCount = 1;
    writes[8].pImageInfo = &irradianceInfo;

    writes[9].dstSet = *descriptorSets_[0];
    writes[9].dstBinding = 10;
    writes[9].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[9].descriptorCount = 1;
    writes[9].pImageInfo = &brdfInfo;

    writes[10].dstSet = *descriptorSets_[1];
    writes[10].dstBinding = 0;
    writes[10].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[10].descriptorCount = 1;
    writes[10].pImageInfo = &displaySampledInfo;

    writes[11].dstSet = *descriptorSets_[1];
    writes[11].dstBinding = 1;
    writes[11].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[11].descriptorCount = 1;
    writes[11].pImageInfo = &hudInfo;

    writes[12].dstSet = *descriptorSets_[0];
    writes[12].dstBinding = 11;
    writes[12].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[12].descriptorCount = 1;
    writes[12].pBufferInfo = &envSampleInfo;

    writes[13].dstSet = *descriptorSets_[0];
    writes[13].dstBinding = 12;
    writes[13].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[13].descriptorCount = 1;
    writes[13].pImageInfo = &textureSrgbInfo;

    writes[14].dstSet = *descriptorSets_[0];
    writes[14].dstBinding = 13;
    writes[14].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[14].descriptorCount = 1;
    writes[14].pImageInfo = &linearTextureInfo;

    writes[15].dstSet = *descriptorSets_[0];
    writes[15].dstBinding = 14;
    writes[15].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[15].descriptorCount = 1;
    writes[15].pBufferInfo = &texSlotInfo;

    writes[16].dstSet = *descriptorSets_[0];
    writes[16].dstBinding = 15;
    writes[16].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[16].descriptorCount = 1;
    writes[16].pBufferInfo = &texTransformInfo;

    device_.updateDescriptorSets(writes, nullptr);
}
void VulkanApp::destroySwapchain() {
    swapchainImageViews_.clear();
    swapchainImages_.clear();
    swapchain_.clear();
}

void VulkanApp::recreateSwapchain() {
    device_.waitIdle();
    destroyRenderFinishedSemaphores();
    swapchainImageViews_.clear();

    createSwapchain();
    createImageViews();
    createRenderFinishedSemaphores();
    createComputeTarget();
    updateDescriptorSets();
}

void VulkanApp::render() {
    vk::Fence fence = *inFlightFences_[currentFrame_];
    std::array<vk::Fence, 1> fences = {fence};
    (void)device_.waitForFences(fences, vk::True, UINT64_MAX);

    uint32_t imageIndex = 0;
    try {
        auto acquire = swapchain_.acquireNextImage(
            UINT64_MAX, *imageAvailableSemaphores_[currentFrame_], nullptr);
        if (acquire.result == vk::Result::eSuboptimalKHR) {
            recreateSwapchain();
            return;
        }
        imageIndex = acquire.value;
    } catch (const vk::OutOfDateKHRError&) {
        recreateSwapchain();
        return;
    }

    (void)device_.resetFences(fences);

    auto& commandBuffer = commandBuffers_[currentFrame_];
    commandBuffer.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    static const auto startTime = std::chrono::steady_clock::now();
    static auto lastFrameTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    float deltaTime =
        std::chrono::duration<float>(now - lastFrameTime).count();
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }
    lastFrameTime = now;
    updateCamera(deltaTime);
    updateHud();

    const float timeSeconds =
        std::chrono::duration<float>(now - startTime).count();

    struct PushConstants {
        float time;
        int32_t frameIndex;
        float cameraPosition[3];
        float cameraForward[3];
        float cameraRight[3];
        float cameraUp[3];
        float exposure;
        int32_t showLightGizmos;
        float environmentRotation;
        float environmentIntensity;
        float environmentPdfScale;
        int32_t environmentSampleWidth;
        int32_t environmentSampleHeight;
        float environmentPad;
    };
    const PushConstants pushConstants{
        timeSeconds,
        static_cast<int32_t>(frameIndex_),
        {cameraPosition_.x, cameraPosition_.y, cameraPosition_.z},
        {cameraForward_.x, cameraForward_.y, cameraForward_.z},
        {cameraRight_.x, cameraRight_.y, cameraRight_.z},
        {cameraUp_.x, cameraUp_.y, cameraUp_.z},
        exposure_,
        showLightGizmos_ ? 1 : 0,
        environmentRotation_,
        environmentIntensity_,
        environmentPdfScale_,
        environmentSampleWidth_,
        environmentSampleHeight_,
        0.0f};

    vk::ImageMemoryBarrier2 hudToTransfer{};
    hudToTransfer.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    hudToTransfer.srcAccessMask = {};
    hudToTransfer.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    hudToTransfer.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
    hudToTransfer.oldLayout = vk::ImageLayout::eUndefined;
    hudToTransfer.newLayout = vk::ImageLayout::eGeneral;
    hudToTransfer.image = *hudImage_;
    hudToTransfer.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    vk::DependencyInfo hudDependency{};
    hudDependency.imageMemoryBarrierCount = 1;
    hudDependency.pImageMemoryBarriers = &hudToTransfer;
    commandBuffer.pipelineBarrier2(hudDependency);

    vk::BufferImageCopy hudCopy{};
    hudCopy.imageSubresource = vk::ImageSubresourceLayers{
        vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    hudCopy.imageExtent = vk::Extent3D(HUD_WIDTH, HUD_HEIGHT, 1);
    commandBuffer.copyBufferToImage(
        *hudStagingBuffer_,
        *hudImage_,
        vk::ImageLayout::eGeneral,
        hudCopy);

    vk::ImageMemoryBarrier2 hudToShader{};
    hudToShader.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    hudToShader.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    hudToShader.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    hudToShader.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    hudToShader.oldLayout = vk::ImageLayout::eGeneral;
    hudToShader.newLayout = vk::ImageLayout::eGeneral;
    hudToShader.image = *hudImage_;
    hudToShader.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    hudDependency.pImageMemoryBarriers = &hudToShader;
    commandBuffer.pipelineBarrier2(hudDependency);

    vk::ImageMemoryBarrier2 toAccum{};
    if (frameIndex_ == 0) {
        toAccum.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        toAccum.oldLayout = vk::ImageLayout::eUndefined;
    } else {
        toAccum.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
        toAccum.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
        toAccum.oldLayout = vk::ImageLayout::eGeneral;
    }
    toAccum.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    toAccum.dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
    toAccum.newLayout = vk::ImageLayout::eGeneral;
    toAccum.image = *accumImage_;
    toAccum.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    vk::ImageMemoryBarrier2 toDisplay{};
    toDisplay.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    toDisplay.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    toDisplay.dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
    toDisplay.oldLayout = vk::ImageLayout::eUndefined;
    toDisplay.newLayout = vk::ImageLayout::eGeneral;
    toDisplay.image = *displayImage_;
    toDisplay.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    std::array<vk::ImageMemoryBarrier2, 2> preDispatchBarriers = {
        toAccum, toDisplay};
    vk::DependencyInfo dependency{};
    dependency.imageMemoryBarrierCount =
        static_cast<uint32_t>(preDispatchBarriers.size());
    dependency.pImageMemoryBarriers = preDispatchBarriers.data();
    commandBuffer.pipelineBarrier2(dependency);

    commandBuffer.bindPipeline(
        vk::PipelineBindPoint::eCompute, *computePipeline_);
    std::array<vk::DescriptorSet, 1> computeSet = {*descriptorSets_[0]};
    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        *computePipelineLayout_,
        0,
        computeSet,
        nullptr);
    commandBuffer.pushConstants(
        *computePipelineLayout_,
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(PushConstants),
        &pushConstants);

    const uint32_t groupX = (swapchainExtent_.width + 7u) / 8u;
    const uint32_t groupY = (swapchainExtent_.height + 7u) / 8u;
    commandBuffer.dispatch(groupX, groupY, 1);

    vk::ImageMemoryBarrier2 toFragment{};
    toFragment.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    toFragment.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
    toFragment.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    toFragment.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
    toFragment.oldLayout = vk::ImageLayout::eGeneral;
    toFragment.newLayout = vk::ImageLayout::eGeneral;
    toFragment.image = *displayImage_;
    toFragment.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &toFragment;
    commandBuffer.pipelineBarrier2(dependency);

    vk::ImageMemoryBarrier2 toColor{};
    toColor.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
    toColor.dstStageMask =
        vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toColor.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toColor.oldLayout = vk::ImageLayout::eUndefined;
    toColor.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    toColor.image = swapchainImages_[imageIndex];
    toColor.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    dependency.pImageMemoryBarriers = &toColor;
    commandBuffer.pipelineBarrier2(dependency);

    vk::ClearValue clearValue(
        vk::ClearColorValue(0.08f, 0.10f, 0.16f, 1.0f));

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.imageView = *swapchainImageViews_[imageIndex];
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = clearValue;

    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D({0, 0}, swapchainExtent_);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    commandBuffer.beginRendering(renderingInfo);

    std::array<vk::Viewport, 1> viewports = {vk::Viewport(
        0.0f,
        0.0f,
        static_cast<float>(swapchainExtent_.width),
        static_cast<float>(swapchainExtent_.height),
        0.0f,
        1.0f)};
    commandBuffer.setViewport(0, viewports);

    std::array<vk::Rect2D, 1> scissors = {
        vk::Rect2D({0, 0}, swapchainExtent_)};
    commandBuffer.setScissor(0, scissors);

    commandBuffer.bindPipeline(
        vk::PipelineBindPoint::eGraphics, *graphicsPipeline_);
    std::array<vk::DescriptorSet, 1> graphicsSet = {*descriptorSets_[1]};
    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *graphicsPipelineLayout_,
        0,
        graphicsSet,
        nullptr);

    const int32_t showHud = showHud_ ? 1 : 0;
    commandBuffer.pushConstants(
        *graphicsPipelineLayout_,
        vk::ShaderStageFlagBits::eFragment,
        0,
        sizeof(int32_t),
        &showHud);

    commandBuffer.draw(3, 1, 0, 0);
    commandBuffer.endRendering();

    vk::ImageMemoryBarrier2 toPresent{};
    toPresent.srcStageMask =
        vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    toPresent.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
    toPresent.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    toPresent.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    toPresent.newLayout = vk::ImageLayout::ePresentSrcKHR;
    toPresent.image = swapchainImages_[imageIndex];
    toPresent.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    dependency.pImageMemoryBarriers = &toPresent;
    commandBuffer.pipelineBarrier2(dependency);

    commandBuffer.end();

    vk::PipelineStageFlags waitStage =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore waitSemaphore = *imageAvailableSemaphores_[currentFrame_];
    vk::Semaphore signalSemaphore = *renderFinishedSemaphores_[imageIndex];
    vk::CommandBuffer rawCommandBuffer = *commandBuffer;

    vk::SubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &rawCommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;
    graphicsQueue_.submit(submitInfo, fence);

    vk::SwapchainKHR rawSwapchain = *swapchain_;
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &signalSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &rawSwapchain;
    presentInfo.pImageIndices = &imageIndex;

    vk::Result result = presentQueue_.presentKHR(presentInfo);
    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR) {
        recreateSwapchain();
    } else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    if (result == vk::Result::eSuccess) {
        ++frameIndex_;
    }
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}