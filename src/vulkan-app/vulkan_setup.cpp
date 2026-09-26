#include "vulkan_common.h"

void VulkanApp::init() {
    bool modelLoaded = false;
    const std::string glbPath = std::string(ASSET_DIR) + "/model.glb";
    const std::string objPath = std::string(ASSET_DIR) + "/model.obj";

    if (std::filesystem::exists(glbPath)) {
        try {
            mesh_ = minitracer::loadGltf(glbPath);
            logMessage("Loaded glTF model: " + glbPath);
            modelLoaded = true;
        } catch (const std::exception& e) {
            logMessage(std::string("glTF load failed: ") + e.what());
        }
    }

    if (!modelLoaded && std::filesystem::exists(objPath)) {
        try {
            mesh_ = minitracer::loadObj(objPath);
            logMessage("Loaded OBJ model: " + objPath);
            modelLoaded = true;
        } catch (const std::exception& e) {
            logMessage(std::string("OBJ load failed: ") + e.what());
        }
    }

    if (!modelLoaded) {
        logMessage("No model loaded, using demo triangle.");
        mesh_ = minitracer::make_demo_triangle();
    }
    minitracer::buildBvh(mesh_);

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    printDeviceInfo();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createDescriptorSetLayouts();
    createGraphicsPipeline();
    createComputePipeline();
    createCommandPool();
    createCommandBuffers();
    createMeshBuffers();
    createTexture();
    createEnvironment();
    createHud();
    createSyncObjects();
    createComputeTarget();
    createSampler();
    createDescriptorPool();
    allocateDescriptorSets();
    updateDescriptorSets();
}

void VulkanApp::cleanup() {
    try {
        if (*device_) {
            device_.waitIdle();
        }
    } catch (...) {
    }
}

bool VulkanApp::validationLayerAvailable() const {
    auto layers = context_.enumerateInstanceLayerProperties();
    for (const auto& layer : layers) {
        if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            return true;
        }
    }
    return false;
}

void VulkanApp::createInstance() {
    vk::ApplicationInfo appInfo(
        "Vulkan-Playground", 1, "No Engine", 1, VK_API_VERSION_1_3);

    std::vector<const char*> extensions = {
        vk::KHRSurfaceExtensionName,
        vk::KHRWin32SurfaceExtensionName
    };
    std::vector<const char*> layers;

    validationEnabled_ = validationLayerAvailable();
    if (validationEnabled_) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    logMessage(validationEnabled_ ? "Validation layer: enabled"
                                  : "Validation layer: not available");

    vk::InstanceCreateInfo createInfo(
        vk::InstanceCreateFlags{},
        &appInfo,
        static_cast<uint32_t>(layers.size()),
        layers.data(),
        static_cast<uint32_t>(extensions.size()),
        extensions.data());

    instance_ = vk::raii::Instance(context_, createInfo);
}

void VulkanApp::setupDebugMessenger() {
    if (!validationEnabled_) {
        return;
    }

    vk::DebugUtilsMessengerCreateInfoEXT info{};
    info.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    info.messageType =
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    info.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(&VulkanApp::debugCallback);

    debugMessenger_ = instance_.createDebugUtilsMessengerEXT(info);
}

void VulkanApp::createSurface() {
    vk::Win32SurfaceCreateInfoKHR info{};
    info.hinstance = GetModuleHandle(nullptr);
    info.hwnd = hwnd_;
    surface_ = instance_.createWin32SurfaceKHR(info);
}

VulkanApp::QueueFamilyIndices VulkanApp::findQueueFamilies(
    vk::PhysicalDevice device) const {
    QueueFamilyIndices indices;
    auto families = device.getQueueFamilyProperties();

    for (uint32_t i = 0; i < static_cast<uint32_t>(families.size()); ++i) {
        const auto required = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
        if ((families[i].queueFlags & required) == required) {
            indices.graphics = i;
        }
        if (device.getSurfaceSupportKHR(i, *surface_)) {
            indices.present = i;
        }
        if (indices.complete()) {
            break;
        }
    }
    return indices;
}

void VulkanApp::pickPhysicalDevice() {
    vk::raii::PhysicalDevices devices(instance_);
    for (auto& device : devices) {
        if (!findQueueFamilies(device).complete()) {
            continue;
        }
        auto extensions = device.enumerateDeviceExtensionProperties();
        bool hasSwapchain = false;
        for (const auto& ext : extensions) {
            if (std::strcmp(ext.extensionName, vk::KHRSwapchainExtensionName) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (hasSwapchain) {
            physicalDevice_ = std::move(device);
            return;
        }
    }
    throw std::runtime_error("No suitable Vulkan GPU found.");
}

void VulkanApp::printDeviceInfo() {
    auto props = physicalDevice_.getProperties();

    const char* typeName = "Unknown";
    switch (props.deviceType) {
        case vk::PhysicalDeviceType::eIntegratedGpu:
            typeName = "Integrated GPU";
            break;
        case vk::PhysicalDeviceType::eDiscreteGpu:
            typeName = "Discrete GPU";
            break;
        case vk::PhysicalDeviceType::eVirtualGpu:
            typeName = "Virtual GPU";
            break;
        case vk::PhysicalDeviceType::eCpu:
            typeName = "CPU";
            break;
        default:
            break;
    }

    std::ostringstream stream;
    std::string deviceName(props.deviceName.data());
    stream << "Selected GPU: " << deviceName << '\n'
           << "Device type : " << typeName << '\n'
           << "API version : " << VK_VERSION_MAJOR(props.apiVersion) << '.'
           << VK_VERSION_MINOR(props.apiVersion) << '.'
           << VK_VERSION_PATCH(props.apiVersion);
    logMessage(stream.str());

    std::wstring gpuName(deviceName.begin(), deviceName.end());
    gpuName_ = gpuName;
}

void VulkanApp::updateWindowTitle() {
    if (!hwnd_) {
        return;
    }

    std::wstringstream stream;
    stream << L"Vulkan-Playground | " << gpuName_
           << L" | " << swapchainExtent_.width << L"x"
           << swapchainExtent_.height
           << L" | 6 bounces | progressive";
    windowTitle_ = stream.str();
    SetWindowTextW(hwnd_, windowTitle_.c_str());
}
void VulkanApp::createLogicalDevice() {
    auto indices = findQueueFamilies(*physicalDevice_);

    std::vector<uint32_t> uniqueFamilies = {indices.graphics, indices.present};
    std::sort(uniqueFamilies.begin(), uniqueFamilies.end());
    uniqueFamilies.erase(
        std::unique(uniqueFamilies.begin(), uniqueFamilies.end()),
        uniqueFamilies.end());

    std::vector<float> priorities(uniqueFamilies.size(), 1.0f);
    std::vector<vk::DeviceQueueCreateInfo> queueInfos;
    for (size_t i = 0; i < uniqueFamilies.size(); ++i) {
        queueInfos.emplace_back(
            vk::DeviceQueueCreateFlags{},
            uniqueFamilies[i],
            1,
            &priorities[i]);
    }

    auto features = physicalDevice_.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan13Features>();
    auto& features13 = features.get<vk::PhysicalDeviceVulkan13Features>();
    if (!features13.dynamicRendering || !features13.synchronization2) {
        throw std::runtime_error("Vulkan 1.3 dynamicRendering/synchronization2 not supported.");
    }

    vk::PhysicalDeviceVulkan13Features enabled13{};
    enabled13.dynamicRendering = vk::True;
    enabled13.synchronization2 = vk::True;

    const std::vector<const char*> deviceExtensions = {
        vk::KHRSwapchainExtensionName
    };

    vk::DeviceCreateInfo deviceInfo(
        vk::DeviceCreateFlags{},
        static_cast<uint32_t>(queueInfos.size()),
        queueInfos.data(),
        0,
        nullptr,
        static_cast<uint32_t>(deviceExtensions.size()),
        deviceExtensions.data(),
        nullptr,
        &enabled13);

    device_ = physicalDevice_.createDevice(deviceInfo);
    graphicsQueue_ = device_.getQueue(indices.graphics, 0);
    presentQueue_ = device_.getQueue(indices.present, 0);
}

VulkanApp::SwapchainSupport VulkanApp::querySwapchainSupport(
    vk::PhysicalDevice device) const {
    SwapchainSupport details;
    details.caps = device.getSurfaceCapabilitiesKHR(*surface_);
    details.formats = device.getSurfaceFormatsKHR(*surface_);
    details.modes = device.getSurfacePresentModesKHR(*surface_);
    return details;
}

vk::SurfaceFormatKHR VulkanApp::chooseSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& formats) const {
    for (const auto& format : formats) {
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }
    return formats[0];
}

vk::PresentModeKHR VulkanApp::choosePresentMode(
    const std::vector<vk::PresentModeKHR>& modes) const {
    for (const auto mode : modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanApp::chooseExtent(
    const vk::SurfaceCapabilitiesKHR& caps) const {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }

    RECT rect{};
    GetClientRect(hwnd_, &rect);
    vk::Extent2D extent{
        static_cast<uint32_t>(rect.right - rect.left),
        static_cast<uint32_t>(rect.bottom - rect.top)};
    extent.width = std::clamp(
        extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(
        extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

void VulkanApp::createSwapchain() {
    auto support = querySwapchainSupport(*physicalDevice_);
    auto format = chooseSurfaceFormat(support.formats);
    auto mode = choosePresentMode(support.modes);
    auto extent = chooseExtent(support.caps);

    uint32_t imageCount = support.caps.minImageCount + 1;
    if (support.caps.maxImageCount > 0 &&
        imageCount > support.caps.maxImageCount) {
        imageCount = support.caps.maxImageCount;
    }

    auto indices = findQueueFamilies(*physicalDevice_);
    std::array<uint32_t, 2> families = {indices.graphics, indices.present};

    vk::SwapchainCreateInfoKHR info{};
    info.surface = *surface_;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    if (indices.graphics != indices.present) {
        info.imageSharingMode = vk::SharingMode::eConcurrent;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = families.data();
    } else {
        info.imageSharingMode = vk::SharingMode::eExclusive;
    }
    info.preTransform = support.caps.currentTransform;
    info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    info.presentMode = mode;
    info.clipped = vk::True;
    info.oldSwapchain = *swapchain_;

    swapchain_ = device_.createSwapchainKHR(info);
    swapchainFormat_ = format.format;
    swapchainExtent_ = extent;
    updateWindowTitle();
    swapchainImages_ = swapchain_.getImages();
}

void VulkanApp::createImageViews() {
    swapchainImageViews_.clear();
    for (const auto image : swapchainImages_) {
        vk::ImageViewCreateInfo info{};
        info.image = image;
        info.viewType = vk::ImageViewType::e2D;
        info.format = swapchainFormat_;
        info.components = vk::ComponentMapping{
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity};
        info.subresourceRange = vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        swapchainImageViews_.push_back(device_.createImageView(info));
    }
}

void VulkanApp::createDescriptorSetLayouts() {
    std::array<vk::DescriptorSetLayoutBinding, 15> computeBindings{};
    computeBindings[0].binding = 0;
    computeBindings[0].descriptorType = vk::DescriptorType::eStorageImage;
    computeBindings[0].descriptorCount = 1;
    computeBindings[0].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[1].binding = 1;
    computeBindings[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[1].descriptorCount = 1;
    computeBindings[1].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[2].binding = 2;
    computeBindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[2].descriptorCount = 1;
    computeBindings[2].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[3].binding = 3;
    computeBindings[3].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[3].descriptorCount = 1;
    computeBindings[3].stageFlags = vk::ShaderStageFlagBits::eCompute;


    computeBindings[4].binding = 5;
    computeBindings[4].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[4].descriptorCount = 1;
    computeBindings[4].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[5].binding = 6;
    computeBindings[5].descriptorType = vk::DescriptorType::eStorageImage;
    computeBindings[5].descriptorCount = 1;
    computeBindings[5].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[6].binding = 7;
    computeBindings[6].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    computeBindings[6].descriptorCount = 1;
    computeBindings[6].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[7].binding = 8;
    computeBindings[7].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[7].descriptorCount = 1;
    computeBindings[7].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[8].binding = 9;
    computeBindings[8].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    computeBindings[8].descriptorCount = 1;
    computeBindings[8].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[9].binding = 10;
    computeBindings[9].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    computeBindings[9].descriptorCount = 1;
    computeBindings[9].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[10].binding = 11;
    computeBindings[10].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[10].descriptorCount = 1;
    computeBindings[10].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[11].binding = 12;
    computeBindings[11].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    computeBindings[11].descriptorCount = 1;
    computeBindings[11].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[12].binding = 13;
    computeBindings[12].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    computeBindings[12].descriptorCount = 1;
    computeBindings[12].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[13].binding = 14;
    computeBindings[13].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[13].descriptorCount = 1;
    computeBindings[13].stageFlags = vk::ShaderStageFlagBits::eCompute;

    computeBindings[14].binding = 15;
    computeBindings[14].descriptorType = vk::DescriptorType::eStorageBuffer;
    computeBindings[14].descriptorCount = 1;
    computeBindings[14].stageFlags = vk::ShaderStageFlagBits::eCompute;

    vk::DescriptorSetLayoutCreateInfo computeInfo{};
    computeInfo.bindingCount = static_cast<uint32_t>(computeBindings.size());
    computeInfo.pBindings = computeBindings.data();
    computeSetLayout_ = device_.createDescriptorSetLayout(computeInfo);

    std::array<vk::DescriptorSetLayoutBinding, 2> graphicsBindings{};
    graphicsBindings[0].binding = 0;
    graphicsBindings[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    graphicsBindings[0].descriptorCount = 1;
    graphicsBindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

    graphicsBindings[1].binding = 1;
    graphicsBindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    graphicsBindings[1].descriptorCount = 1;
    graphicsBindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo graphicsInfo{};
    graphicsInfo.bindingCount = static_cast<uint32_t>(graphicsBindings.size());
    graphicsInfo.pBindings = graphicsBindings.data();
    graphicsSetLayout_ = device_.createDescriptorSetLayout(graphicsInfo);
}

vk::raii::ShaderModule VulkanApp::loadShader(const char* filename) const {
    const std::string path = std::string(SHADER_DIR) + "/" + filename;
    auto code = readFile(path);

    vk::ShaderModuleCreateInfo info{};
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());
    return device_.createShaderModule(info);
}

void VulkanApp::createGraphicsPipeline() {
    auto vertModule = loadShader("triangle.vert.spv");
    auto fragModule = loadShader("triangle.frag.spv");

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *vertModule;
    stages[0].pName = "main";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *fragModule;
    stages[1].pName = "main";

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PushConstantRange hudPushRange{};
    hudPushRange.stageFlags = vk::ShaderStageFlagBits::eFragment;
    hudPushRange.offset = 0;
    hudPushRange.size = sizeof(int32_t);

    vk::PipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &*graphicsSetLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &hudPushRange;
    graphicsPipelineLayout_ = device_.createPipelineLayout(layoutInfo);

    vk::PipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &swapchainFormat_;

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = *graphicsPipelineLayout_;
    pipelineInfo.renderPass = nullptr;

    graphicsPipeline_ = device_.createGraphicsPipeline(nullptr, pipelineInfo);
}

void VulkanApp::createComputePipeline() {
    auto computeModule = loadShader("raytrace.comp.spv");

    vk::PipelineShaderStageCreateInfo stage{};
    stage.stage = vk::ShaderStageFlagBits::eCompute;
    stage.module = *computeModule;
    stage.pName = "main";

    vk::PushConstantRange pushRange{};
    pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
    pushRange.offset = 0;
    pushRange.size = 88;

    vk::PipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &*computeSetLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    computePipelineLayout_ = device_.createPipelineLayout(layoutInfo);

    vk::ComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.stage = stage;
    pipelineInfo.layout = *computePipelineLayout_;
    computePipeline_ = device_.createComputePipeline(nullptr, pipelineInfo);
}

void VulkanApp::createCommandPool() {
    vk::CommandPoolCreateInfo info{};
    info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    info.queueFamilyIndex = findQueueFamilies(*physicalDevice_).graphics;
    commandPool_ = device_.createCommandPool(info);
}

void VulkanApp::createCommandBuffers() {
    vk::CommandBufferAllocateInfo info{};
    info.commandPool = *commandPool_;
    info.level = vk::CommandBufferLevel::ePrimary;
    info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    commandBuffers_ = device_.allocateCommandBuffers(info);
}

