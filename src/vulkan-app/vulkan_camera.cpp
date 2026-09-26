#include "vulkan_common.h"

void VulkanApp::updateCameraBasis() {
    const float cp = std::cos(cameraPitch_);
    const float sp = std::sin(cameraPitch_);
    const float cy = std::cos(cameraYaw_);
    const float sy = std::sin(cameraYaw_);

    cameraPosition_ = {
        cameraTarget_.x + cameraDistance_ * cp * sy,
        cameraTarget_.y + cameraDistance_ * sp,
        cameraTarget_.z + cameraDistance_ * cp * cy};

    cameraForward_ =
        vec3Normalize(vec3Sub(cameraTarget_, cameraPosition_));

    const minitracer::Vec3 worldUp{0.0f, 1.0f, 0.0f};
    cameraRight_ =
        vec3Normalize(vec3Cross(cameraForward_, worldUp));
    cameraUp_ =
        vec3Normalize(vec3Cross(cameraRight_, cameraForward_));
}

void VulkanApp::resetCamera() {
    cameraTarget_ = {0.0f, 0.0f, 0.0f};
    cameraYaw_ = 0.0f;
    cameraPitch_ = 0.2f;
    cameraDistance_ = 2.5f;
    updateCameraBasis();
    frameIndex_ = 0;
}

void VulkanApp::onLeftMouseDown() {
    leftDragging_ = true;
    POINT point{};
    GetCursorPos(&point);
    ScreenToClient(hwnd_, &point);
    lastMouseX_ = point.x;
    lastMouseY_ = point.y;
}

void VulkanApp::onLeftMouseUp() {
    leftDragging_ = false;
}

void VulkanApp::onRightMouseDown() {
    rightDragging_ = true;
    POINT point{};
    GetCursorPos(&point);
    ScreenToClient(hwnd_, &point);
    lastMouseX_ = point.x;
    lastMouseY_ = point.y;
}

void VulkanApp::onRightMouseUp() {
    rightDragging_ = false;
}

void VulkanApp::onMouseMove(int x, int y) {
    if (leftDragging_) {
        const int dx = x - lastMouseX_;
        const int dy = y - lastMouseY_;
        lastMouseX_ = x;
        lastMouseY_ = y;

        cameraYaw_ += static_cast<float>(dx) * 0.005f;
        cameraPitch_ -= static_cast<float>(dy) * 0.005f;
        cameraPitch_ = std::clamp(cameraPitch_, -1.5f, 1.5f);
        updateCameraBasis();
        frameIndex_ = 0;
    } else if (rightDragging_) {
        const int dx = x - lastMouseX_;
        const int dy = y - lastMouseY_;
        lastMouseX_ = x;
        lastMouseY_ = y;

        const float scale = 0.002f * cameraDistance_;
        cameraTarget_.x -= cameraRight_.x * dx * scale;
        cameraTarget_.y -= cameraRight_.y * dx * scale;
        cameraTarget_.z -= cameraRight_.z * dx * scale;
        cameraTarget_.x += cameraUp_.x * dy * scale;
        cameraTarget_.y += cameraUp_.y * dy * scale;
        cameraTarget_.z += cameraUp_.z * dy * scale;
        updateCameraBasis();
        frameIndex_ = 0;
    }
}

void VulkanApp::onMouseWheel(int wheelDelta) {
    const float factor = std::pow(0.9f, static_cast<float>(wheelDelta) / 120.0f);
    cameraDistance_ = std::clamp(cameraDistance_ * factor, 0.5f, 50.0f);
    updateCameraBasis();
    frameIndex_ = 0;
}

void VulkanApp::onKeyDown(unsigned int key) {
    if (key < 256) {
        keyDown_[key] = true;
    }
    if (key == 'R') {
        resetCamera();
    } else if (key == 'L') {
        showLightGizmos_ = !showLightGizmos_;
    } else if (key == 'H') {
        showHud_ = !showHud_;
    } else if (key == VK_OEM_4) {
        exposure_ = std::clamp(exposure_ * 0.9f, 0.05f, 10.0f);
    } else if (key == VK_OEM_6) {
        exposure_ = std::clamp(exposure_ * 1.1f, 0.05f, 10.0f);
    } else if (key == VK_OEM_PERIOD) {
        environmentRotation_ += 0.1f;
        frameIndex_ = 0;
    } else if (key == VK_OEM_COMMA) {
        environmentRotation_ -= 0.1f;
        frameIndex_ = 0;
    } else if (key == VK_OEM_7) {
        environmentIntensity_ =
            std::clamp(environmentIntensity_ * 1.1f, 0.05f, 10.0f);
        frameIndex_ = 0;
    } else if (key == VK_OEM_1) {
        environmentIntensity_ =
            std::clamp(environmentIntensity_ * 0.9f, 0.05f, 10.0f);
        frameIndex_ = 0;
    }
}

void VulkanApp::onKeyUp(unsigned int key) {
    if (key < 256) {
        keyDown_[key] = false;
    }
}

void VulkanApp::updateCamera(float deltaTime) {
    float speed = 2.0f * deltaTime;
    if (keyDown_[VK_CONTROL]) {
        speed *= 3.0f;
    }

    minitracer::Vec3 move{0.0f, 0.0f, 0.0f};
    if (keyDown_['W']) {
        move.x += cameraForward_.x;
        move.y += cameraForward_.y;
        move.z += cameraForward_.z;
    }
    if (keyDown_['S']) {
        move.x -= cameraForward_.x;
        move.y -= cameraForward_.y;
        move.z -= cameraForward_.z;
    }
    if (keyDown_['D']) {
        move.x += cameraRight_.x;
        move.y += cameraRight_.y;
        move.z += cameraRight_.z;
    }
    if (keyDown_['A']) {
        move.x -= cameraRight_.x;
        move.y -= cameraRight_.y;
        move.z -= cameraRight_.z;
    }
    if (keyDown_[VK_SPACE]) {
        move.x += cameraUp_.x;
        move.y += cameraUp_.y;
        move.z += cameraUp_.z;
    }
    if (keyDown_[VK_SHIFT]) {
        move.x -= cameraUp_.x;
        move.y -= cameraUp_.y;
        move.z -= cameraUp_.z;
    }

    if (vec3Length(move) > 1e-6f) {
        const auto normalized = vec3Normalize(move);
        cameraTarget_.x += normalized.x * speed;
        cameraTarget_.y += normalized.y * speed;
        cameraTarget_.z += normalized.z * speed;
        updateCameraBasis();
        frameIndex_ = 0;
    }
}
