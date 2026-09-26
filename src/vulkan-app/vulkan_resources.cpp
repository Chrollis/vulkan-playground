#include "vulkan_common.h"

void VulkanApp::createMeshBuffers() {
    createDeviceLocalBuffer(
        mesh_.vertices.data(),
        mesh_.vertices.size() * sizeof(minitracer::Vertex),
        vertexBuffer_,
        vertexBufferMemory_);
    createDeviceLocalBuffer(
        mesh_.triangles.data(),
        mesh_.triangles.size() * sizeof(minitracer::Triangle),
        triangleBuffer_,
        triangleBufferMemory_);
    createDeviceLocalBuffer(
        mesh_.materials.data(),
        mesh_.materials.size() * sizeof(minitracer::Material),
        materialBuffer_,
        materialBufferMemory_);
    createDeviceLocalBuffer(
        mesh_.bvhNodes.data(),
        mesh_.bvhNodes.size() * sizeof(minitracer::BVHNode),
        bvhBuffer_,
        bvhBufferMemory_);

    // A single directional light. Edit this block to change the lighting setup.
    std::vector<minitracer::GPULight> lights(1);
    lights[0].type = 0;
    lights[0].direction[0] = 0.4f;
    lights[0].direction[1] = 0.8f;
    lights[0].direction[2] = 0.3f;
    lights[0].color[0] = 1.0f;
    lights[0].color[1] = 0.98f;
    lights[0].color[2] = 0.95f;
    lights[0].intensity = 2.0f;

    createDeviceLocalBuffer(
        lights.data(),
        lights.size() * sizeof(minitracer::GPULight),
        lightBuffer_,
        lightBufferMemory_);
}
