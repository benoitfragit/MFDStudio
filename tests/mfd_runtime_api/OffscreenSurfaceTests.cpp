/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for the public offscreen surface API.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#ifndef APIENTRY
#define APIENTRY WINAPI
#endif
extern "C" __declspec(dllimport) HDC APIENTRY wglGetCurrentDC();
extern "C" __declspec(dllimport) HGLRC APIENTRY wglGetCurrentContext();
#endif

#include <raylib.h>
#include <rlgl.h>

#include "internal/RuntimeSessionInternalAccess.hpp"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"
#include "mfd/runtime_api/OffscreenSurface.h"
#include "mfd/runtime_api/RuntimeSession.h"

namespace
{
constexpr int kRenderSize = 96;

std::filesystem::path RepositoryRoot()
{
    const std::filesystem::path testFile = std::filesystem::path(__FILE__).lexically_normal();
    return testFile.parent_path().parent_path().parent_path();
}

std::filesystem::path TutorialWindowFile()
{
    return RepositoryRoot() / "examples/tutorial/assets/windows/mfd_tutorial.json";
}

bool TutorialAssetsAreMissing(const std::filesystem::path& windowFile)
{
    if (!std::filesystem::exists(windowFile))
    {
        return true;
    }

    return false;
}

mfd::ColorRgba Green() noexcept
{
    return {0, 255, 0, 255};
}

mfd::ColorRgba Red() noexcept
{
    return {255, 0, 0, 255};
}

mfd::Primitive MakeFilledRectangle(const std::string& id, const mfd::ColorRgba color)
{
    mfd::Primitive primitive;
    primitive.id = id;
    primitive.type = mfd::PrimitiveType::Rectangle;
    primitive.geometry = mfd::RectangleGeometry {1.60f, 1.60f};
    primitive.style.color = color;
    primitive.style.fillColor = color;
    primitive.style.filled = true;
    return primitive;
}

mfd::Primitive MakeInvisibleCircleMask()
{
    mfd::Primitive primitive;
    primitive.id = "mask";
    primitive.type = mfd::PrimitiveType::Circle;
    primitive.geometry = mfd::CircleGeometry {0.35f};
    primitive.style.visible = false;
    return primitive;
}

mfd::MfdDocument MakeSolidDocument(const mfd::ColorRgba color)
{
    mfd::ReticleGroup reticle;
    reticle.id = "solid";
    reticle.primitives.push_back(MakeFilledRectangle("body", color));

    mfd::PageDefinition page;
    page.name = "Main";
    page.normalizedName = mfd::NormalizePageName(page.name);
    page.title = page.name;
    page.defaultPage = true;
    page.backgroundColor = {0, 0, 0, 255};
    page.staticReticles.push_back(std::move(reticle));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));
    return document;
}

mfd::MfdDocument MakeMaskedDocument(const mfd::ReticleClipMode clipMode)
{
    mfd::ReticleGroup backdrop;
    backdrop.id = "backdrop";
    backdrop.primitives.push_back(MakeFilledRectangle("background", Green()));

    mfd::ReticleGroup reticle;
    reticle.id = "mask";
    reticle.clipping.mode = clipMode;
    reticle.clipping.primitiveId = "mask";
    reticle.primitives.push_back(MakeInvisibleCircleMask());

    mfd::PageDefinition page;
    page.name = "Main";
    page.normalizedName = mfd::NormalizePageName(page.name);
    page.title = page.name;
    page.defaultPage = true;
    page.backgroundColor = {0, 0, 0, 255};
    page.staticReticles.push_back(std::move(backdrop));
    page.staticReticles.push_back(std::move(reticle));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));
    return document;
}

void OverrideSessionDocument(mfd::runtime_api::RuntimeSession& session, mfd::MfdDocument document)
{
    mfd::SceneRegistry& scene = mfd::runtime_api::internal::RuntimeSessionInternalAccess::Scene(session);
    scene.LoadDocument(std::move(document));
    scene.SetActivePage("Main");
}

std::uint8_t PixelChannel(const mfd::runtime_api::OffscreenFrameView& frame,
                          const int x,
                          const int y,
                          const int channel)
{
    const std::size_t rowOffset =
        static_cast<std::size_t>(y) * frame.rowStrideBytes + static_cast<std::size_t>(x) * 4U;
    return frame.pixels[rowOffset + static_cast<std::size_t>(channel)];
}

void ExpectGreen(const mfd::runtime_api::OffscreenFrameView& frame, const int x, const int y)
{
    EXPECT_LT(PixelChannel(frame, x, y, 0), 20U);
    EXPECT_GT(PixelChannel(frame, x, y, 1), 200U);
    EXPECT_LT(PixelChannel(frame, x, y, 2), 20U);
}

void ExpectRed(const mfd::runtime_api::OffscreenFrameView& frame, const int x, const int y)
{
    EXPECT_GT(PixelChannel(frame, x, y, 0), 200U);
    EXPECT_LT(PixelChannel(frame, x, y, 1), 20U);
    EXPECT_LT(PixelChannel(frame, x, y, 2), 20U);
}

void ExpectBlack(const mfd::runtime_api::OffscreenFrameView& frame, const int x, const int y)
{
    EXPECT_LT(PixelChannel(frame, x, y, 0), 20U);
    EXPECT_LT(PixelChannel(frame, x, y, 1), 20U);
    EXPECT_LT(PixelChannel(frame, x, y, 2), 20U);
}

void ExpectBlueDominant(const mfd::runtime_api::OffscreenFrameView& frame, const int x, const int y)
{
    EXPECT_LT(PixelChannel(frame, x, y, 0), 32U);
    EXPECT_LT(PixelChannel(frame, x, y, 1), 64U);
    EXPECT_GT(PixelChannel(frame, x, y, 2), 64U);
}

void ExpectOpaque(const mfd::runtime_api::OffscreenFrameView& frame, const int x, const int y)
{
    EXPECT_GT(PixelChannel(frame, x, y, 3), 200U);
}

void ExpectGreen(const Color pixel)
{
    EXPECT_LT(pixel.r, 20);
    EXPECT_GT(pixel.g, 200);
    EXPECT_LT(pixel.b, 20);
}

#if defined(_WIN32)
struct NativeGlContextHandle
{
    HDC deviceContext = nullptr;
    HGLRC renderContext = nullptr;
};

NativeGlContextHandle CaptureCurrentNativeGlContext() noexcept
{
    return {wglGetCurrentDC(), wglGetCurrentContext()};
}

bool SameNativeGlContext(const NativeGlContextHandle& lhs, const NativeGlContextHandle& rhs) noexcept
{
    return lhs.deviceContext == rhs.deviceContext && lhs.renderContext == rhs.renderContext;
}
#endif
} // namespace

TEST(OffscreenSurfaceTests, ResizeUsesRequestedSurfaceSizeWithoutVisibleHostWindow)
{
    mfd::runtime_api::OffscreenSurface surface;
    ASSERT_TRUE(surface.Resize(80, 48));
    EXPECT_TRUE(surface.Ready());
    EXPECT_EQ(surface.Width(), 80);
    EXPECT_EQ(surface.Height(), 48);
}

TEST(OffscreenSurfaceTests, ClippingRemainsValidWhenRenderedOffscreen)
{
    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(RepositoryRoot() / "examples/demo/assets/windows/demo_window.json", error)) << error;
    OverrideSessionDocument(session, MakeMaskedDocument(mfd::ReticleClipMode::Outer));

    mfd::runtime_api::OffscreenSurface surface(kRenderSize, kRenderSize);
    ASSERT_TRUE(surface.Render(session));

    const mfd::runtime_api::OffscreenFrameView frame = surface.FrameView();
    ASSERT_TRUE(frame.Ready());
    EXPECT_EQ(frame.width, kRenderSize);
    EXPECT_EQ(frame.height, kRenderSize);
    ExpectGreen(frame, 48, 48);
    ExpectBlack(frame, 12, 12);
}

TEST(OffscreenSurfaceTests, TwoSurfacesKeepIndependentFrames)
{
    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(RepositoryRoot() / "examples/demo/assets/windows/demo_window.json", error)) << error;

    mfd::runtime_api::OffscreenSurface leftSurface(kRenderSize, kRenderSize);
    mfd::runtime_api::OffscreenSurface rightSurface(kRenderSize, kRenderSize);

    OverrideSessionDocument(session, MakeSolidDocument(Green()));
    ASSERT_TRUE(leftSurface.Render(session));

    OverrideSessionDocument(session, MakeSolidDocument(Red()));
    ASSERT_TRUE(rightSurface.Render(session));

    const mfd::runtime_api::OffscreenFrameView leftFrame = leftSurface.FrameView();
    const mfd::runtime_api::OffscreenFrameView rightFrame = rightSurface.FrameView();
    ASSERT_TRUE(leftFrame.Ready());
    ASSERT_TRUE(rightFrame.Ready());
    ASSERT_NE(leftFrame.pixels, rightFrame.pixels);

    ExpectGreen(leftFrame, 24, 24);
    ExpectRed(rightFrame, 24, 24);
}

TEST(OffscreenSurfaceTests, SurfaceDestructionAfterHostWindowCloseDoesNotRequireLiveContext)
{
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "mfd_runtime_api_surface_lifetime");
        ASSERT_TRUE(IsWindowReady());

        const mfd::runtime_api::OffscreenSurface surface(32, 32);
        ASSERT_TRUE(surface.Ready());

        CloseWindow();
    }

    SUCCEED();
}

TEST(OffscreenSurfaceTests, TutorialWindowRendersVisiblePixelsOffscreen)
{
    const std::filesystem::path windowFile = TutorialWindowFile();
    if (TutorialAssetsAreMissing(windowFile))
    {
        GTEST_SKIP() << "Tutorial assets have not been generated yet: " << windowFile.string();
    }

    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(windowFile, error)) << error;

    mfd::runtime_api::OffscreenSurface surface(kRenderSize, kRenderSize);
    ASSERT_TRUE(surface.Render(session));

    const mfd::runtime_api::OffscreenFrameView frame = surface.FrameView();
    ASSERT_TRUE(frame.Ready());
    ExpectOpaque(frame, kRenderSize / 2, kRenderSize / 2);
}

TEST(OffscreenSurfaceTests, TutorialWindowRendersWithExistingHostWindow)
{
    const std::filesystem::path windowFile = TutorialWindowFile();
    if (TutorialAssetsAreMissing(windowFile))
    {
        GTEST_SKIP() << "Tutorial assets have not been generated yet: " << windowFile.string();
    }

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(320, 240, "mfd_runtime_api_host_window");
    ASSERT_TRUE(IsWindowReady());

    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(windowFile, error)) << error;

    mfd::runtime_api::OffscreenSurface surface(kRenderSize, kRenderSize);
    ASSERT_TRUE(surface.Render(session));

    const mfd::runtime_api::OffscreenFrameView frame = surface.FrameView();
    ASSERT_TRUE(frame.Ready());
    ExpectOpaque(frame, kRenderSize / 2, kRenderSize / 2);

    CloseWindow();
}

TEST(OffscreenSurfaceTests, TutorialFrameUploadsIntoConsumerTexture)
{
    const std::filesystem::path windowFile = TutorialWindowFile();
    if (TutorialAssetsAreMissing(windowFile))
    {
        GTEST_SKIP() << "Tutorial assets have not been generated yet: " << windowFile.string();
    }

    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "mfd_runtime_api_texture_upload");
    ASSERT_TRUE(IsWindowReady());

    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(windowFile, error)) << error;

    mfd::runtime_api::OffscreenSurface surface(kRenderSize, kRenderSize);
    ASSERT_TRUE(surface.Render(session));

    const mfd::runtime_api::OffscreenFrameView frame = surface.FrameView();
    ASSERT_TRUE(frame.Ready());

    const Image seedImage = GenImageColor(frame.width, frame.height, BLACK);
    Texture2D texture = LoadTextureFromImage(seedImage);
    UnloadImage(seedImage);
    ASSERT_NE(texture.id, 0U);

    UpdateTexture(texture, frame.pixels);

    const Image uploadedImage = LoadImageFromTexture(texture);
    ASSERT_NE(uploadedImage.data, nullptr);
    const Color uploadedPixel = GetImageColor(uploadedImage, frame.width / 2, frame.height / 2);
    EXPECT_GT(uploadedPixel.a, 200);

    UnloadImage(uploadedImage);
    UnloadTexture(texture);
    CloseWindow();
}

TEST(OffscreenSurfaceTests, HostRaylibCanPresentOffscreenFrameWithoutBlackOutput)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "mfd_runtime_api_host_presentation");
    ASSERT_TRUE(IsWindowReady());

    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(RepositoryRoot() / "examples/demo/assets/windows/demo_window.json", error)) << error;
    OverrideSessionDocument(session, MakeSolidDocument(Green()));

    mfd::runtime_api::OffscreenSurface surface(kRenderSize, kRenderSize);
    ASSERT_TRUE(surface.Render(session));

    const mfd::runtime_api::OffscreenFrameView frame = surface.FrameView();
    ASSERT_TRUE(frame.Ready());

    const Image seedImage = GenImageColor(frame.width, frame.height, BLACK);
    Texture2D presentedTexture = LoadTextureFromImage(seedImage);
    UnloadImage(seedImage);
    ASSERT_NE(presentedTexture.id, 0U);
    UpdateTexture(presentedTexture, frame.pixels);

    RenderTexture2D hostTarget = LoadRenderTexture(frame.width, frame.height);
    ASSERT_NE(hostTarget.id, 0U);

    BeginTextureMode(hostTarget);
    ClearBackground(BLACK);
    DrawTexturePro(
        presentedTexture,
        Rectangle {0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)},
        Rectangle {0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)},
        Vector2 {0.0f, 0.0f},
        0.0f,
        WHITE);
    EndTextureMode();

    const Image hostImage = LoadImageFromTexture(hostTarget.texture);
    ASSERT_NE(hostImage.data, nullptr);
    ExpectGreen(GetImageColor(hostImage, frame.width / 2, frame.height / 2));

    UnloadImage(hostImage);
    UnloadRenderTexture(hostTarget);
    UnloadTexture(presentedTexture);
    CloseWindow();
}

#if defined(_WIN32)
TEST(OffscreenSurfaceTests, OffscreenRenderRestoresHostNativeGlContext)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "mfd_runtime_api_context_restore");
    ASSERT_TRUE(IsWindowReady());

    mfd::runtime_api::RuntimeSession session;
    std::string error;
    ASSERT_TRUE(session.LoadWindowFile(RepositoryRoot() / "examples/demo/assets/windows/demo_window.json", error)) << error;
    OverrideSessionDocument(session, MakeSolidDocument(Green()));

    const NativeGlContextHandle hostContextBeforeRender = CaptureCurrentNativeGlContext();
    mfd::runtime_api::OffscreenSurface surface(kRenderSize, kRenderSize);
    ASSERT_TRUE(surface.Render(session));
    const NativeGlContextHandle hostContextAfterRender = CaptureCurrentNativeGlContext();
    EXPECT_TRUE(SameNativeGlContext(hostContextBeforeRender, hostContextAfterRender));
    CloseWindow();
}
#endif
