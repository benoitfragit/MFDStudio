/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the offscreen MFD runtime host (raylib backend).
 */

#include "OffscreenMfdView.h"

#include <algorithm>

#include <raylib.h>

namespace lhld
{
OffscreenMfdView::OffscreenMfdView() = default;

OffscreenMfdView::~OffscreenMfdView()
{
    Shutdown();
}

bool OffscreenMfdView::Initialize(const std::filesystem::path& windowFile, std::string& error)
{
    if (!renderContextReady_)
    {
        // A hidden raylib window only exists to own the OpenGL context the
        // offscreen surface renders into; it is never shown or pumped.
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "lhld_offscreen");
        renderContextReady_ = IsWindowReady();
        if (!renderContextReady_)
        {
            error = "Unable to create the hidden offscreen render context";
            return false;
        }
    }

    if (!session_.LoadWindowFile(windowFile, error))
    {
        return false;
    }

    return true;
}

bool OffscreenMfdView::SetActivePage(const std::string_view pageName)
{
    return session_.SetActivePage(pageName);
}

void OffscreenMfdView::Update(const float deltaSeconds, const int width, const int height)
{
    const int clampedWidth = std::max(1, width);
    const int clampedHeight = std::max(1, height);
    surface_.Resize(clampedWidth, clampedHeight);
    session_.Advance(deltaSeconds);
    surface_.Render(session_);
}

OffscreenPixels OffscreenMfdView::Pixels() const noexcept
{
    const mfd::runtime_api::OffscreenFrameView frame = surface_.FrameView();
    OffscreenPixels pixels;
    pixels.pixels = frame.pixels;
    pixels.width = frame.width;
    pixels.height = frame.height;
    pixels.rowStrideBytes = frame.rowStrideBytes;
    pixels.ready = frame.Ready();
    return pixels;
}

std::string OffscreenMfdView::ActivePageName() const
{
    return session_.ActivePageName();
}

std::string OffscreenMfdView::LastCommandStatus() const
{
    return session_.LastCommandStatus();
}

void OffscreenMfdView::Shutdown() noexcept
{
    surface_.Release();
    if (renderContextReady_ && IsWindowReady())
    {
        CloseWindow();
    }
    renderContextReady_ = false;
}
} // namespace lhld
