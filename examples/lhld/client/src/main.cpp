/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Entry point for the interactive LHLD demonstration client.
 */

#include <exception>
#include <iostream>
#include <string>

#include "MfdApplication.h"
#include "Win32Dx11ImGuiHost.h"

int main()
{
    try
    {
        Win32Dx11ImGuiHost host;
        Win32Dx11ImGuiHost::Config hostConfig;
        hostConfig.width = 1280;
        hostConfig.height = 860;
        hostConfig.minWidth = 900;
        hostConfig.minHeight = 720;
        hostConfig.title = "LHLD - Left Avionics Console";

        std::string error;
        if (!host.Initialize(hostConfig, error))
        {
            std::cerr << "Unable to initialize the DX11 ImGui host: " << error << '\n';
            return 1;
        }

        lhld::MfdApplication application;
        if (!application.Initialize(host.Device(), host.DeviceContext(), error))
        {
            std::cerr << "Unable to initialize the LHLD demo: " << error << '\n';
            return 1;
        }

        host.SetClearColor(6, 10, 14, 255);
        float deltaSeconds = 0.0f;
        // The host owns the native event loop; the application receives a
        // sanitized per-frame delta, publishes MFD commands and renders the
        // hosted runtime offscreen into the bezel.
        while (host.BeginFrame(deltaSeconds))
        {
            application.DrawFrame(deltaSeconds);
            host.EndFrame();
        }

        application.Shutdown();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
