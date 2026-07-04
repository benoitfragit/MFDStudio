/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Entry point for the interactive Demo HUD demonstration client.
 */

#include <exception>
#include <iostream>
#include <string>

#include "DemoHudApplication.h"
#include "Win32Dx11ImGuiHost.h"

int main()
{
    try
    {
        Win32Dx11ImGuiHost host;
        Win32Dx11ImGuiHost::Config hostConfig;
        // Keep the operator panel compact enough to sit beside the HUD window
        // while preserving a usable minimum size for the large control buttons.
        hostConfig.width = 760;
        hostConfig.height = 820;
        hostConfig.minWidth = 620;
        hostConfig.minHeight = 600;
        hostConfig.title = "Demo HUD Controls";

        std::string error;
        if (!host.Initialize(hostConfig, error))
        {
            std::cerr << "Unable to initialize the DX11 ImGui host: " << error << '\n';
            return 1;
        }

        demo_hud::DemoHudApplication application;
        if (!application.Initialize(error))
        {
            std::cerr << "Unable to initialize the Demo HUD demo: " << error << '\n';
            return 1;
        }

        host.SetClearColor(8, 13, 18, 255);
        float deltaSeconds = 0.0f;
        // The host owns the native event loop; the application only receives a
        // sanitized per-frame delta and submits HUD commands before presentation.
        while (host.BeginFrame(deltaSeconds))
        {
            application.DrawFrame(deltaSeconds);
            host.EndFrame();
        }

        // Send one explicit shutdown caption to the HUD runtime before the UDP
        // publisher is destroyed.
        application.Shutdown();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
