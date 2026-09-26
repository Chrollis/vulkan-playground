#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <imm.h>
#include <exception>
#include <fstream>

#include "vulkan_app.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"Vulkan-PlaygroundWindow";
constexpr wchar_t kWindowTitle[] = L"Vulkan-Playground";
constexpr int kDefaultWidth = 960;
constexpr int kDefaultHeight = 720;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<VulkanApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
            } else if (app) {
                app->onKeyDown(static_cast<unsigned int>(wParam));
            }
            return 0;

        case WM_KEYUP:
            if (app) {
                app->onKeyUp(static_cast<unsigned int>(wParam));
            }
            return 0;

        case WM_LBUTTONDOWN:
            if (app) {
                app->onLeftMouseDown();
                SetCapture(hwnd);
            }
            return 0;

        case WM_LBUTTONUP:
            if (app) {
                app->onLeftMouseUp();
                ReleaseCapture();
            }
            return 0;

        case WM_RBUTTONDOWN:
            if (app) {
                app->onRightMouseDown();
                SetCapture(hwnd);
            }
            return 0;

        case WM_RBUTTONUP:
            if (app) {
                app->onRightMouseUp();
                ReleaseCapture();
            }
            return 0;

        case WM_MOUSEMOVE: {
            const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
            if (app) {
                app->onMouseMove(x, y);
            }
            return 0;
        }

        case WM_MOUSEWHEEL:
            if (app) {
                app->onMouseWheel(static_cast<short>(HIWORD(wParam)));
            }
            return 0;

        case WM_IME_SETCONTEXT:
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_CHAR:
        case WM_IME_NOTIFY:
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Vulkan-Playground", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kDefaultWidth,
        kDefaultHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create window.", L"Vulkan-Playground", MB_ICONERROR);
        return 1;
    }

    ImmAssociateContext(hwnd, nullptr);
    ImmDisableIME(0);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    VulkanApp app(hwnd);
    try {
        app.init();
    } catch (const std::exception& e) {
        std::ofstream("Vulkan-Playground.log", std::ios::app) << "Init exception: " << e.what() << std::endl;
        MessageBoxA(hwnd, e.what(), "Vulkan-Playground", MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }

    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));

    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (running && !IsIconic(hwnd)) {
            try {
                app.render();
            } catch (const std::exception& e) {
                std::ofstream("Vulkan-Playground.log", std::ios::app) << "Render exception: " << e.what() << std::endl;
                MessageBoxA(hwnd, e.what(), "Vulkan-Playground", MB_ICONERROR);
                running = false;
            }
        } else if (running) {
            WaitMessage();
        }
    }

    app.cleanup();
    return 0;
}
