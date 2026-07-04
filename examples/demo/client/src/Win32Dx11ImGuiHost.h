/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Win32 + DX11 host used by the MFD demo client.
 */

#include <cstdint>
#include <memory>
#include <string>

/**
 * @brief Minimal Dear ImGui shell backed by Win32 and Direct3D 11.
 *
 * @details This host is intentionally local to `demo_client`.
 * It keeps their business logic focused on the public client/runtime APIs.
 */
class Win32Dx11ImGuiHost
{
public:
    /**
     * @brief Window-creation settings consumed during initialization.
     */
    struct Config
    {
        int width = 1500;
        int height = 920;
        int minWidth = 1180;
        int minHeight = 720;
        std::string title = "MFD Demo Client";
    };

    Win32Dx11ImGuiHost();
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
     * @param red Red channel in `[0, 255]`.
     * @param green Green channel in `[0, 255]`.
     * @param blue Blue channel in `[0, 255]`.
     * @param alpha Alpha channel in `[0, 255]`.
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
     * @brief Returns the elapsed shell uptime in seconds.
     * @return Monotonic elapsed time since `Initialize()`.
     */
    double TimeSeconds() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_ {};
};
