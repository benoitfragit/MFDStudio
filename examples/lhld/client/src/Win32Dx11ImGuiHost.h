/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Win32 + DX11 host used by the LHLD demonstration client.
 */

#include <cstdint>
#include <memory>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;

/**
 * @brief Minimal Dear ImGui shell backed by Win32 and Direct3D 11.
 *
 * @details Local to `lhld_client`. It also exposes the DX11 device and
 * immediate context so the client can upload the offscreen-rendered MFD image
 * into a shader-resource texture drawn inside the ImGui bezel.
 */
class Win32Dx11ImGuiHost
{
public:
    /**
     * @brief Window-creation settings consumed during initialization.
     */
    struct Config
    {
        int width = 900;
        int height = 940;
        int minWidth = 760;
        int minHeight = 760;
        std::string title = "LHLD Client";
    };

    Win32Dx11ImGuiHost();

    /**
     * @brief Releases ImGui backends, the native window and all DX11 resources.
     */
    ~Win32Dx11ImGuiHost();

    Win32Dx11ImGuiHost(const Win32Dx11ImGuiHost&) = delete;
    Win32Dx11ImGuiHost& operator=(const Win32Dx11ImGuiHost&) = delete;

    Win32Dx11ImGuiHost(Win32Dx11ImGuiHost&&) noexcept;
    Win32Dx11ImGuiHost& operator=(Win32Dx11ImGuiHost&&) noexcept;

    /**
     * @brief Creates the native window, the DX11 device, and the ImGui backends.
     * @param config Requested shell configuration.
     * @param error Human-readable error populated on failure.
     * @return `true` when the host is ready to render frames.
     */
    bool Initialize(const Config& config, std::string& error);

    /**
     * @brief Pumps pending OS messages and starts one new ImGui frame.
     * @param deltaSeconds Elapsed wall-clock time since the previous frame.
     * @return `true` while the shell should keep running.
     */
    bool BeginFrame(float& deltaSeconds);

    /**
     * @brief Sets the background color used by the next present.
     */
    void SetClearColor(std::uint8_t red,
                       std::uint8_t green,
                       std::uint8_t blue,
                       std::uint8_t alpha) noexcept;

    /**
     * @brief Renders the current ImGui draw data and presents the swap chain.
     */
    void EndFrame();

    /**
     * @brief Returns the DX11 device, or nullptr before initialization.
     */
    ID3D11Device* Device() const noexcept;

    /**
     * @brief Returns the DX11 immediate context, or nullptr before initialization.
     */
    ID3D11DeviceContext* DeviceContext() const noexcept;

    /**
     * @brief Returns the elapsed shell uptime in seconds.
     */
    double TimeSeconds() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_ {};
};
