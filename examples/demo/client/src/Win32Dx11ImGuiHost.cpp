/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the private Win32 + DX11 host used by `demo_client`.
 */

#include "Win32Dx11ImGuiHost.h"

#include <chrono>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <tchar.h>
#endif

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#if defined(_WIN32)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace
{
#if defined(_WIN32)
std::wstring ToWideString(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

    const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (requiredSize <= 0)
    {
        return std::wstring(value.begin(), value.end());
    }

    std::wstring wide(static_cast<std::size_t>(requiredSize - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), requiredSize);
    return wide;
}
#endif
} // namespace

struct Win32Dx11ImGuiHost::Impl
{
#if defined(_WIN32)
    using Clock = std::chrono::steady_clock;

    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND windowHandle = nullptr;
    WNDCLASSEXW windowClass {};
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* deviceContext = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    Clock::time_point startTime {};
    Clock::time_point lastFrameTime {};
    UINT pendingWidth = 0;
    UINT pendingHeight = 0;
    int minWidth = 0;
    int minHeight = 0;
    bool resizePending = false;
    bool initialized = false;
    bool closeRequested = false;
    float clearColor[4] {8.0f / 255.0f, 13.0f / 255.0f, 18.0f / 255.0f, 1.0f};

    static constexpr const wchar_t* kWindowClassName = L"MfdDemoClientWin32Dx11Host";

    static LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam))
        {
            return 1;
        }

        Impl* impl = nullptr;
        if (message == WM_NCCREATE)
        {
            const auto* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            impl = reinterpret_cast<Impl*>(createStruct->lpCreateParams);
            if (impl != nullptr)
            {
                SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
            }
        }
        else
        {
            impl = reinterpret_cast<Impl*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
        }

        switch (message)
        {
        case WM_SIZE:
            if (impl != nullptr && impl->device != nullptr && wParam != SIZE_MINIMIZED)
            {
                impl->pendingWidth = LOWORD(lParam);
                impl->pendingHeight = HIWORD(lParam);
                impl->resizePending = true;
            }
            return 0;

        case WM_GETMINMAXINFO:
            if (impl != nullptr)
            {
                auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
                info->ptMinTrackSize.x = impl->minWidth;
                info->ptMinTrackSize.y = impl->minHeight;
                return 0;
            }
            break;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0u) == SC_KEYMENU)
            {
                return 0;
            }
            break;

        case WM_DESTROY:
            if (impl != nullptr)
            {
                impl->closeRequested = true;
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(windowHandle, message, wParam, lParam);
    }

    void ReleaseRenderTarget() noexcept
    {
        if (renderTargetView != nullptr)
        {
            renderTargetView->Release();
            renderTargetView = nullptr;
        }
    }

    void CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;
        if (swapChain == nullptr ||
            FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) ||
            backBuffer == nullptr)
        {
            throw std::runtime_error("Unable to access the DX11 back buffer");
        }

        const HRESULT createResult = device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
        backBuffer->Release();
        if (FAILED(createResult) || renderTargetView == nullptr)
        {
            throw std::runtime_error("Unable to create the DX11 render target view");
        }
    }

    void ReleaseDevice() noexcept
    {
        ReleaseRenderTarget();

        if (swapChain != nullptr)
        {
            swapChain->Release();
            swapChain = nullptr;
        }

        if (deviceContext != nullptr)
        {
            deviceContext->Release();
            deviceContext = nullptr;
        }

        if (device != nullptr)
        {
            device->Release();
            device = nullptr;
        }
    }

    bool CreateDevice(HWND hostWindow, std::string& error)
    {
        DXGI_SWAP_CHAIN_DESC swapChainDescription {};
        swapChainDescription.BufferCount = 2;
        swapChainDescription.BufferDesc.Width = 0;
        swapChainDescription.BufferDesc.Height = 0;
        swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDescription.BufferDesc.RefreshRate.Numerator = 60;
        swapChainDescription.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDescription.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDescription.OutputWindow = hostWindow;
        swapChainDescription.SampleDesc.Count = 1;
        swapChainDescription.SampleDesc.Quality = 0;
        swapChainDescription.Windowed = TRUE;
        swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        constexpr D3D_FEATURE_LEVEL featureLevels[] {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };
        constexpr UINT createFlags = 0;

        D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        const HRESULT hardwareResult = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createFlags,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &swapChainDescription,
            &swapChain,
            &device,
            &createdFeatureLevel,
            &deviceContext);

        if (FAILED(hardwareResult))
        {
            const HRESULT warpResult = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                createFlags,
                featureLevels,
                static_cast<UINT>(std::size(featureLevels)),
                D3D11_SDK_VERSION,
                &swapChainDescription,
                &swapChain,
                &device,
                &createdFeatureLevel,
                &deviceContext);
            if (FAILED(warpResult))
            {
                error = "Unable to create the DX11 device or swap chain";
                return false;
            }
        }

        try
        {
            CreateRenderTarget();
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            ReleaseDevice();
            return false;
        }

        return true;
    }

    void HandlePendingResize()
    {
        if (!resizePending || swapChain == nullptr)
        {
            return;
        }

        ReleaseRenderTarget();
        const HRESULT resizeResult = swapChain->ResizeBuffers(0, pendingWidth, pendingHeight, DXGI_FORMAT_UNKNOWN, 0);
        resizePending = false;
        pendingWidth = 0;
        pendingHeight = 0;

        if (FAILED(resizeResult))
        {
            throw std::runtime_error("Unable to resize the DX11 swap chain");
        }

        CreateRenderTarget();
    }

    void Shutdown() noexcept
    {
        if (!initialized)
        {
            return;
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        ReleaseDevice();

        if (windowHandle != nullptr)
        {
            DestroyWindow(windowHandle);
            windowHandle = nullptr;
        }

        if (windowClass.lpszClassName != nullptr)
        {
            UnregisterClassW(windowClass.lpszClassName, instance);
        }

        windowClass = {};
        closeRequested = false;
        initialized = false;
    }
#endif
};

Win32Dx11ImGuiHost::Win32Dx11ImGuiHost()
    : impl_(std::make_unique<Impl>())
{
}

Win32Dx11ImGuiHost::~Win32Dx11ImGuiHost()
{
#if defined(_WIN32)
    if (impl_ != nullptr)
    {
        impl_->Shutdown();
    }
#endif
}

Win32Dx11ImGuiHost::Win32Dx11ImGuiHost(Win32Dx11ImGuiHost&&) noexcept = default;

Win32Dx11ImGuiHost& Win32Dx11ImGuiHost::operator=(Win32Dx11ImGuiHost&&) noexcept = default;

bool Win32Dx11ImGuiHost::Initialize([[maybe_unused]] const Config& config, std::string& error)
{
#if defined(_WIN32)
    if (impl_ == nullptr)
    {
        error = "Host implementation is unavailable";
        return false;
    }

    impl_->Shutdown();
    impl_->minWidth = config.minWidth;
    impl_->minHeight = config.minHeight;

    impl_->windowClass = {};
    impl_->windowClass.cbSize = sizeof(WNDCLASSEXW);
    impl_->windowClass.style = CS_CLASSDC;
    impl_->windowClass.lpfnWndProc = &Impl::WindowProc;
    impl_->windowClass.hInstance = impl_->instance;
    impl_->windowClass.lpszClassName = Impl::kWindowClassName;

    if (RegisterClassExW(&impl_->windowClass) == 0)
    {
        error = "Unable to register the Win32 window class";
        return false;
    }

    const std::wstring title = ToWideString(config.title);
    impl_->windowHandle = CreateWindowW(
        impl_->windowClass.lpszClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        config.width,
        config.height,
        nullptr,
        nullptr,
        impl_->instance,
        impl_.get());

    if (impl_->windowHandle == nullptr)
    {
        error = "Unable to create the Win32 shell window";
        impl_->Shutdown();
        return false;
    }

    if (!impl_->CreateDevice(impl_->windowHandle, error))
    {
        impl_->Shutdown();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplWin32_Init(impl_->windowHandle);
    ImGui_ImplDX11_Init(impl_->device, impl_->deviceContext);

    ShowWindow(impl_->windowHandle, SW_SHOWDEFAULT);
    UpdateWindow(impl_->windowHandle);

    impl_->startTime = Impl::Clock::now();
    impl_->lastFrameTime = impl_->startTime;
    impl_->closeRequested = false;
    impl_->initialized = true;
    return true;
#else
    error = "Win32 + DX11 host is only available on Windows";
    return false;
#endif
}

bool Win32Dx11ImGuiHost::BeginFrame([[maybe_unused]] float& deltaSeconds)
{
#if defined(_WIN32)
    deltaSeconds = 0.0f;
    if (impl_ == nullptr || !impl_->initialized)
    {
        return false;
    }

    MSG message {};
    while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (message.message == WM_QUIT)
        {
            impl_->closeRequested = true;
        }
    }

    if (impl_->closeRequested)
    {
        return false;
    }

    impl_->HandlePendingResize();

    const Impl::Clock::time_point now = Impl::Clock::now();
    deltaSeconds = std::chrono::duration<float>(now - impl_->lastFrameTime).count();
    impl_->lastFrameTime = now;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    return true;
#else
    return false;
#endif
}

void Win32Dx11ImGuiHost::SetClearColor([[maybe_unused]] const std::uint8_t red,
                                       [[maybe_unused]] const std::uint8_t green,
                                       [[maybe_unused]] const std::uint8_t blue,
                                       [[maybe_unused]] const std::uint8_t alpha) noexcept
{
#if defined(_WIN32)
    if (impl_ == nullptr)
    {
        return;
    }

    impl_->clearColor[0] = static_cast<float>(red) / 255.0f;
    impl_->clearColor[1] = static_cast<float>(green) / 255.0f;
    impl_->clearColor[2] = static_cast<float>(blue) / 255.0f;
    impl_->clearColor[3] = static_cast<float>(alpha) / 255.0f;
#endif
}

void Win32Dx11ImGuiHost::EndFrame()
{
#if defined(_WIN32)
    if (impl_ == nullptr || !impl_->initialized || impl_->deviceContext == nullptr || impl_->renderTargetView == nullptr)
    {
        return;
    }

    ImGui::Render();
    impl_->deviceContext->OMSetRenderTargets(1, &impl_->renderTargetView, nullptr);
    impl_->deviceContext->ClearRenderTargetView(impl_->renderTargetView, impl_->clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (impl_->swapChain != nullptr)
    {
        impl_->swapChain->Present(1, 0);
    }
#endif
}

double Win32Dx11ImGuiHost::TimeSeconds() const noexcept
{
#if defined(_WIN32)
    if (impl_ == nullptr || !impl_->initialized)
    {
        return 0.0;
    }

    return std::chrono::duration<double>(Impl::Clock::now() - impl_->startTime).count();
#else
    return 0.0;
#endif
}
