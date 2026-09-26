#include "bvh_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

namespace {

using Float3 = std::array<float, 3>;

struct TriangleInfo {
    Float3 boundsMin;
    Float3 boundsMax;
    Float3 centroid;
    uint32_t triangleIndex = 0;
};

Float3 positionOf(const minitracer::Vertex& vertex) {
    return {vertex.position[0], vertex.position[1], vertex.position[2]};
}

Float3 min3(const Float3& a, const Float3& b) {
    return {std::min(a[0], b[0]), std::min(a[1], b[1]), std::min(a[2], b[2])};
}

Float3 max3(const Float3& a, const Float3& b) {
    return {std::max(a[0], b[0]), std::max(a[1], b[1]), std::max(a[2], b[2])};
}

} // namespace

namespace minitracer {

void buildBvh(Mesh& mesh) {
    if (mesh.triangles.empty()) {
        return;
    }

    std::vector<TriangleInfo> infos(mesh.triangles.size());

    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const Triangle& triangle = mesh.triangles[i];
        const Float3 p0 = positionOf(mesh.vertices[triangle.v0]);
        const Float3 p1 = positionOf(mesh.vertices[triangle.v1]);
        const Float3 p2 = positionOf(mesh.vertices[triangle.v2]);

        TriangleInfo info{};
        info.boundsMin = min3(min3(p0, p1), p2);
        info.boundsMax = max3(max3(p0, p1), p2);
        info.centroid = {
            (info.boundsMin[0] + info.boundsMax[0]) * 0.5f,
            (info.boundsMin[1] + info.boundsMax[1]) * 0.5f,
            (info.boundsMin[2] + info.boundsMax[2]) * 0.5f};
        info.triangleIndex = static_cast<uint32_t>(i);
        infos[i] = info;
    }

    std::vector<BVHNode> nodes;
    nodes.reserve(infos.size() * 2);
    std::vector<Triangle> orderedTriangles;
    orderedTriangles.reserve(mesh.triangles.size());

    std::function<uint32_t(size_t, size_t)> build =
        [&](size_t start, size_t end) -> uint32_t {
        Float3 boundsMin = infos[start].boundsMin;
        Float3 boundsMax = infos[start].boundsMax;
        for (size_t i = start + 1; i < end; ++i) {
            boundsMin = min3(boundsMin, infos[i].boundsMin);
            boundsMax = max3(boundsMax, infos[i].boundsMax);
        }

        const uint32_t nodeIndex = static_cast<uint32_t>(nodes.size());
        BVHNode node{};
        node.min[0] = boundsMin[0];
        node.min[1] = boundsMin[1];
        node.min[2] = boundsMin[2];
        node.max[0] = boundsMax[0];
        node.max[1] = boundsMax[1];
        node.max[2] = boundsMax[2];
        nodes.push_back(node);

        constexpr size_t kLeafSize = 8;
        const size_t count = end - start;
        if (count <= kLeafSize) {
            const uint32_t first = static_cast<uint32_t>(orderedTriangles.size());
            for (size_t i = start; i < end; ++i) {
                orderedTriangles.push_back(mesh.triangles[infos[i].triangleIndex]);
            }
            nodes[nodeIndex].left = -1;
            nodes[nodeIndex].right = -1;
            nodes[nodeIndex].start = static_cast<int32_t>(first);
            nodes[nodeIndex].count = static_cast<int32_t>(count);
            return nodeIndex;
        }

        const Float3 extent = {
            boundsMax[0] - boundsMin[0],
            boundsMax[1] - boundsMin[1],
            boundsMax[2] - boundsMin[2]};
        int axis = 0;
        if (extent[1] > extent[axis]) axis = 1;
        if (extent[2] > extent[axis]) axis = 2;

        const size_t mid = start + count / 2;
        std::nth_element(
            infos.begin() + start,
            infos.begin() + mid,
            infos.begin() + end,
            [axis](const TriangleInfo& a, const TriangleInfo& b) {
                return a.centroid[axis] < b.centroid[axis];
            });

        const uint32_t left = build(start, mid);
        const uint32_t right = build(mid, end);
        nodes[nodeIndex].left = static_cast<int32_t>(left);
        nodes[nodeIndex].right = static_cast<int32_t>(right);
        nodes[nodeIndex].start = 0;
        nodes[nodeIndex].count = 0;
        return nodeIndex;
    };

    build(0, infos.size());
    mesh.triangles = std::move(orderedTriangles);
    mesh.bvhNodes = std::move(nodes);
}

} // namespace minitracer