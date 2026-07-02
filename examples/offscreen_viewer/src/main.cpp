/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Resizable example hosting one public runtime session offscreen.
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <raylib.h>

#include "mfd/runtime_api/OffscreenSurface.h"
#include "mfd/runtime_api/RuntimeSession.h"

namespace
{
constexpr int kMinimumWindowWidth = 960;
constexpr int kMinimumWindowHeight = 640;
constexpr int kSidebarWidth = 320;
constexpr int kOuterPadding = 18;
constexpr int kInnerPadding = 12;
constexpr int kPreviewAspectWidth = 4;
constexpr int kPreviewAspectHeight = 3;
constexpr float kPanelLabelHeight = 24.0f;
constexpr char kDefaultWindowFile[] = "assets/windows/demo_pages_minimal.json";

struct CommandLineOptions
{
    std::filesystem::path windowFile = kDefaultWindowFile;
    bool noSnapshot = false;
    bool showHelp = false;
};

struct ViewerLayout
{
    Rectangle mainBounds {};
    Rectangle previewBounds {};
    Rectangle sidebarBounds {};
    int mainTextureWidth = 0;
    int mainTextureHeight = 0;
    int previewTextureWidth = 0;
    int previewTextureHeight = 0;
};

struct PresentedTexture
{
    Texture2D texture {};
    int width = 0;
    int height = 0;
    bool ready = false;
};

struct ResolvedWindowFile
{
    std::filesystem::path path {};
    std::vector<std::filesystem::path> searchedPaths {};
    std::vector<std::filesystem::path> suggestedPaths {};
    bool exists = false;
};

bool ParseCommandLine(const int argc, char** argv, CommandLineOptions& options, std::string& error)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
            return true;
        }

        if (argument == "--window" || argument == "-w")
        {
            if (index + 1 >= argc)
            {
                error = "Missing path after '--window'.";
                return false;
            }

            options.windowFile = argv[++index];
            continue;
        }

        if (argument == "--no-snapshot")
        {
            options.noSnapshot = true;
            continue;
        }

        if (!argument.empty() && argument.front() == '-')
        {
            error = "Unknown argument: " + argument;
            return false;
        }

        options.windowFile = argument;
    }

    return true;
}

std::string BuildUsageText()
{
    std::string usage;
    usage += "Usage:\n";
    usage += "  offscreen_viewer [--window <window.json>]\n";
    usage += "  offscreen_viewer [--window <window.json>] [--no-snapshot]\n";
    usage += "  offscreen_viewer <window.json>\n";
    usage += "  offscreen_viewer --help\n";
    usage += "\n";
    usage += "The example hosts the runtime offscreen through mfd_runtime_api,\n";
    usage += "keeps UDP in/out enabled, and displays two independent images.\n";
    usage += "--no-snapshot keeps earlier commands of one runtime batch applied when a later command fails.\n";
    usage += "Example tutorial asset: assets/windows/mfd_tutorial.json\n";
    return usage;
}

std::filesystem::path NormalizeCandidatePath(const std::filesystem::path& candidate)
{
    return candidate.empty() ? candidate : candidate.lexically_normal();
}

bool PathsEquivalent(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    return NormalizeCandidatePath(lhs) == NormalizeCandidatePath(rhs);
}

void AppendUniquePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& candidate)
{
    if (candidate.empty())
    {
        return;
    }

    for (const std::filesystem::path& existingPath : paths)
    {
        if (PathsEquivalent(existingPath, candidate))
        {
            return;
        }
    }

    paths.push_back(NormalizeCandidatePath(candidate));
}

std::vector<std::filesystem::path> BuildSearchRoots(const std::filesystem::path& executableFile)
{
    std::vector<std::filesystem::path> roots;
    AppendUniquePath(roots, std::filesystem::current_path());
    if (!executableFile.empty())
    {
        AppendUniquePath(roots, executableFile.parent_path());
    }

    return roots;
}

void AppendWindowCandidates(const std::filesystem::path& requestedPath,
                            const std::vector<std::filesystem::path>& roots,
                            std::vector<std::filesystem::path>& candidates)
{
    if (requestedPath.is_absolute())
    {
        AppendUniquePath(candidates, requestedPath);
        return;
    }

    for (const std::filesystem::path& root : roots)
    {
        AppendUniquePath(candidates, root / requestedPath);
    }

    const std::filesystem::path requestedFileName = requestedPath.filename();
    if (!requestedFileName.empty())
    {
        for (const std::filesystem::path& root : roots)
        {
            AppendUniquePath(candidates, root / "assets" / "windows" / requestedFileName);
        }
    }
}

std::vector<std::string> TokenizePathStem(const std::filesystem::path& path)
{
    std::vector<std::string> tokens;
    const std::string stem = path.stem().string();
    std::string currentToken;
    for (const char character : stem)
    {
        if (std::isalnum(static_cast<unsigned char>(character)) != 0)
        {
            currentToken.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        }
        else if (!currentToken.empty())
        {
            if (currentToken.size() >= 4U)
            {
                tokens.push_back(currentToken);
            }
            currentToken.clear();
        }
    }

    if (currentToken.size() >= 4U)
    {
        tokens.push_back(currentToken);
    }

    return tokens;
}

bool SharesMeaningfulToken(const std::filesystem::path& requestedPath, const std::filesystem::path& candidatePath)
{
    const std::vector<std::string> requestedTokens = TokenizePathStem(requestedPath);
    const std::vector<std::string> candidateTokens = TokenizePathStem(candidatePath);
    for (const std::string& requestedToken : requestedTokens)
    {
        for (const std::string& candidateToken : candidateTokens)
        {
            if (requestedToken == candidateToken)
            {
                return true;
            }
        }
    }

    return false;
}

void AppendSuggestedWindows(const std::filesystem::path& requestedPath,
                            const std::vector<std::filesystem::path>& roots,
                            std::vector<std::filesystem::path>& suggestions)
{
    for (const std::filesystem::path& root : roots)
    {
        const std::filesystem::path windowDirectory = root / "assets" / "windows";
        if (!std::filesystem::exists(windowDirectory))
        {
            continue;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(windowDirectory))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            if (SharesMeaningfulToken(requestedPath, entry.path()))
            {
                AppendUniquePath(suggestions, entry.path());
            }
        }
    }
}

ResolvedWindowFile ResolveWindowFile(const std::filesystem::path& requestedPath,
                                     const std::filesystem::path& executableFile)
{
    ResolvedWindowFile resolved;
    const std::vector<std::filesystem::path> roots = BuildSearchRoots(executableFile);
    AppendWindowCandidates(requestedPath, roots, resolved.searchedPaths);
    for (const std::filesystem::path& candidate : resolved.searchedPaths)
    {
        if (std::filesystem::exists(candidate))
        {
            resolved.path = candidate;
            resolved.exists = true;
            return resolved;
        }
    }

    resolved.path = requestedPath;
    AppendSuggestedWindows(requestedPath, roots, resolved.suggestedPaths);
    return resolved;
}

std::string BuildMissingWindowFileError(const std::filesystem::path& requestedPath, const ResolvedWindowFile& resolved)
{
    std::ostringstream stream;
    stream << "Unable to locate window JSON '" << requestedPath.string() << "'.";
    if (!resolved.searchedPaths.empty())
    {
        stream << " Searched:";
        for (const std::filesystem::path& searchedPath : resolved.searchedPaths)
        {
            stream << "\n  - " << searchedPath.string();
        }
    }

    if (!resolved.suggestedPaths.empty())
    {
        stream << "\nDid you mean:";
        for (const std::filesystem::path& suggestedPath : resolved.suggestedPaths)
        {
            stream << "\n  - " << suggestedPath.string();
        }
    }

    return stream.str();
}

std::string BuildWindowTitle(const mfd::runtime_api::RuntimeWindowInfo& windowInfo)
{
    return windowInfo.title + " - offscreen_viewer";
}

void ApplyWindowTiming(const mfd::runtime_api::RuntimeWindowInfo& windowInfo)
{
    if (windowInfo.targetFps > 0)
    {
        SetTargetFPS(windowInfo.targetFps);
    }
}

void HandlePageShortcuts(mfd::runtime_api::RuntimeSession& runtimeSession)
{
    static constexpr std::array<int, 9> kKeyBindings {
        KEY_ONE,
        KEY_TWO,
        KEY_THREE,
        KEY_FOUR,
        KEY_FIVE,
        KEY_SIX,
        KEY_SEVEN,
        KEY_EIGHT,
        KEY_NINE};

    const std::vector<mfd::runtime_api::RuntimePageInfo> pages = runtimeSession.Pages();
    for (std::size_t index = 0; index < pages.size() && index < kKeyBindings.size(); ++index)
    {
        if (IsKeyPressed(kKeyBindings[index]))
        {
            const bool pageActivated = runtimeSession.SetActivePage(pages[index].name);
            if (!pageActivated)
            {
                break;
            }
        }
    }
}

ViewerLayout BuildViewerLayout(const int screenWidth, const int screenHeight)
{
    const int sidebarX = std::max(kOuterPadding, screenWidth - kSidebarWidth - kOuterPadding);
    const int contentWidth = std::max(160, sidebarX - 2 * kOuterPadding);
    const int contentHeight = std::max(160, screenHeight - 2 * kOuterPadding);
    const int previewWidth = std::max(160, std::min(contentWidth / 3, kSidebarWidth - 2 * kInnerPadding));
    const int previewHeight = std::max(120, previewWidth * kPreviewAspectHeight / kPreviewAspectWidth);
    const int mainHeight = std::max(120, contentHeight - previewHeight - kInnerPadding);

    ViewerLayout layout;
    layout.mainBounds = Rectangle {
        static_cast<float>(kOuterPadding),
        static_cast<float>(kOuterPadding),
        static_cast<float>(contentWidth),
        static_cast<float>(mainHeight)};
    layout.previewBounds = Rectangle {
        static_cast<float>(kOuterPadding),
        static_cast<float>(kOuterPadding + mainHeight + kInnerPadding),
        static_cast<float>(previewWidth),
        static_cast<float>(previewHeight)};
    layout.sidebarBounds = Rectangle {
        static_cast<float>(sidebarX),
        static_cast<float>(kOuterPadding),
        static_cast<float>(std::max(220, screenWidth - sidebarX - kOuterPadding)),
        static_cast<float>(std::max(160, screenHeight - 2 * kOuterPadding))};
    layout.mainTextureWidth = std::max(1, contentWidth);
    layout.mainTextureHeight = std::max(1, mainHeight);
    layout.previewTextureWidth = std::max(1, previewWidth);
    layout.previewTextureHeight = std::max(1, previewHeight);
    return layout;
}

Rectangle FitTextureIntoBounds(const Rectangle& bounds, const int textureWidth, const int textureHeight)
{
    if (textureWidth <= 0 || textureHeight <= 0)
    {
        return Rectangle {bounds.x, bounds.y, 0.0f, 0.0f};
    }

    const float scale = std::min(bounds.width / static_cast<float>(textureWidth),
                                 bounds.height / static_cast<float>(textureHeight));
    const float renderWidth = static_cast<float>(textureWidth) * scale;
    const float renderHeight = static_cast<float>(textureHeight) * scale;
    return Rectangle {
        bounds.x + 0.5f * (bounds.width - renderWidth),
        bounds.y + 0.5f * (bounds.height - renderHeight),
        renderWidth,
        renderHeight};
}

void DrawPanelFrame(const Rectangle& bounds, const char* title)
{
    DrawRectangleRounded(bounds, 0.04f, 6, Color {18, 25, 35, 255});
    DrawRectangleRoundedLinesEx(bounds, 0.04f, 6, 1.0f, Color {58, 79, 104, 255});
    DrawText(title,
             static_cast<int>(bounds.x) + 12,
             static_cast<int>(bounds.y) + 8,
             18,
             Color {212, 231, 255, 255});
}

void ReleasePresentedTexture(PresentedTexture& texture) noexcept
{
    if (texture.ready && IsWindowReady())
    {
        UnloadTexture(texture.texture);
    }

    texture.texture = {};
    texture.width = 0;
    texture.height = 0;
    texture.ready = false;
}

bool EnsurePresentedTexture(PresentedTexture& texture, const int width, const int height)
{
    if (width <= 0 || height <= 0)
    {
        ReleasePresentedTexture(texture);
        return false;
    }

    if (texture.ready && texture.width == width && texture.height == height)
    {
        return true;
    }

    ReleasePresentedTexture(texture);

    const Image image = GenImageColor(width, height, BLACK);
    texture.texture = LoadTextureFromImage(image);
    UnloadImage(image);
    texture.ready = texture.texture.id != 0U;
    texture.width = texture.ready ? width : 0;
    texture.height = texture.ready ? height : 0;
    return texture.ready;
}

bool UpdatePresentedTexture(PresentedTexture& texture, const mfd::runtime_api::OffscreenFrameView& frame)
{
    if (!frame.Ready() || !EnsurePresentedTexture(texture, frame.width, frame.height))
    {
        return false;
    }

    UpdateTexture(texture.texture, frame.pixels);
    return true;
}

Rectangle PresentedTextureSource(const PresentedTexture& texture)
{
    return Rectangle {
        0.0f,
        0.0f,
        static_cast<float>(texture.width),
        static_cast<float>(texture.height)};
}

void DrawSurfacePanel(const Rectangle& bounds, const char* title, const PresentedTexture& texture)
{
    DrawPanelFrame(bounds, title);

    const Rectangle contentBounds {
        bounds.x + 12.0f,
        bounds.y + kPanelLabelHeight + 8.0f,
        bounds.width - 24.0f,
        bounds.height - kPanelLabelHeight - 20.0f};

    DrawRectangleRounded(contentBounds, 0.03f, 6, Color {6, 10, 15, 255});

    if (!texture.ready)
    {
        DrawText("Surface not ready.",
                 static_cast<int>(contentBounds.x) + 12,
                 static_cast<int>(contentBounds.y) + 12,
                 18,
                 Color {255, 196, 102, 255});
        return;
    }

    const Rectangle destination = FitTextureIntoBounds(contentBounds, texture.width, texture.height);
    DrawTexturePro(texture.texture,
                   PresentedTextureSource(texture),
                   destination,
                   Vector2 {0.0f, 0.0f},
                   0.0f,
                   WHITE);
}

std::string ClipStatusText(const std::string& value, const std::size_t maximumLength)
{
    if (value.size() <= maximumLength)
    {
        return value;
    }

    return value.substr(0, maximumLength - 3U) + "...";
}

float DrawSidebarLine(const Rectangle& bounds,
                      const float currentY,
                      const std::string& label,
                      const std::string& value,
                      const Color color)
{
    DrawText(label.c_str(), static_cast<int>(bounds.x) + 14, static_cast<int>(currentY), 18, Color {170, 189, 214, 255});
    DrawText(ClipStatusText(value, 44U).c_str(),
             static_cast<int>(bounds.x) + 14,
             static_cast<int>(currentY + 22.0f),
             18,
             color);
    return currentY + 54.0f;
}

void DrawSidebar(const Rectangle& bounds,
                 const mfd::runtime_api::RuntimeSession& runtimeSession,
                 const mfd::runtime_api::OffscreenSurface& mainSurface,
                 const mfd::runtime_api::OffscreenSurface& previewSurface,
                 const std::string& runtimeError)
{
    DrawPanelFrame(bounds, "Runtime status");

    float currentY = bounds.y + 42.0f;
    currentY = DrawSidebarLine(
        bounds,
        currentY,
        "Window JSON",
        runtimeSession.WindowFile().empty() ? std::string {"<none>"} : runtimeSession.WindowFile().string(),
        Color {228, 236, 245, 255});
    currentY = DrawSidebarLine(
        bounds,
        currentY,
        "Active page",
        runtimeSession.ActivePageName().empty() ? std::string {"<none>"} : runtimeSession.ActivePageName(),
        Color {120, 235, 185, 255});
    currentY = DrawSidebarLine(
        bounds,
        currentY,
        "Command status",
        runtimeSession.LastCommandStatus().empty() ? std::string {"<none>"} : runtimeSession.LastCommandStatus(),
        Color {226, 233, 241, 255});
    currentY = DrawSidebarLine(
        bounds,
        currentY,
        "Feedback status",
        runtimeSession.LastFeedbackStatus().empty() ? std::string {"<none>"} : runtimeSession.LastFeedbackStatus(),
        Color {226, 233, 241, 255});
    currentY = DrawSidebarLine(
        bounds,
        currentY,
        "Primary image",
        std::to_string(mainSurface.Width()) + "x" + std::to_string(mainSurface.Height()),
        Color {159, 214, 255, 255});
    DrawSidebarLine(bounds,
                    currentY,
                    "Secondary image",
                    std::to_string(previewSurface.Width()) + "x" + std::to_string(previewSurface.Height()),
                    Color {159, 214, 255, 255});

    DrawText("Keys: R reload, 1..9 switch pages, external UDP clients still work.",
             static_cast<int>(bounds.x) + 14,
             static_cast<int>(bounds.y + bounds.height - 58.0f),
             18,
             Color {180, 196, 216, 255});

    if (!runtimeError.empty())
    {
        DrawText(ClipStatusText(runtimeError, 52U).c_str(),
                 static_cast<int>(bounds.x) + 14,
                 static_cast<int>(bounds.y + bounds.height - 30.0f),
                 18,
                 Color {255, 131, 120, 255});
    }
}

void ApplyLoadedWindowDefinition(const mfd::runtime_api::RuntimeSession& runtimeSession)
{
    const mfd::runtime_api::RuntimeWindowInfo windowInfo = runtimeSession.WindowInfo();
    SetWindowTitle(BuildWindowTitle(windowInfo).c_str());
    ApplyWindowTiming(windowInfo);
}

int MainImpl(const int argc, char** argv)
{
    CommandLineOptions options;
    std::string error;
    if (!ParseCommandLine(argc, argv, options, error))
    {
        std::cerr << error << '\n';
        std::cerr << BuildUsageText();
        return 1;
    }

    if (options.showHelp)
    {
        std::cout << BuildUsageText();
        return 0;
    }

    const ResolvedWindowFile resolvedWindowFile =
        ResolveWindowFile(options.windowFile, argc > 0 ? std::filesystem::absolute(argv[0]) : std::filesystem::path {});
    if (!resolvedWindowFile.exists)
    {
        throw std::runtime_error(BuildMissingWindowFileError(options.windowFile, resolvedWindowFile));
    }
    options.windowFile = resolvedWindowFile.path;

    mfd::runtime_api::RuntimeSession runtimeSession;
    if (options.noSnapshot)
    {
        runtimeSession.SetCommandTransactionMode(mfd::runtime_api::RuntimeCommandTransactionMode::NonTransactional);
    }
    if (!runtimeSession.LoadWindowFile(options.windowFile, error))
    {
        throw std::runtime_error("Unable to load window JSON '" + options.windowFile.string() + "': " + error);
    }

    const mfd::runtime_api::RuntimeWindowInfo windowInfo = runtimeSession.WindowInfo();
    const int initialWidth = std::max(kMinimumWindowWidth, windowInfo.width + kSidebarWidth);
    const int initialHeight = std::max(kMinimumWindowHeight, windowInfo.height);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(initialWidth, initialHeight, BuildWindowTitle(windowInfo).c_str());
    SetWindowMinSize(kMinimumWindowWidth, kMinimumWindowHeight);
    SetExitKey(KEY_NULL);
    ApplyWindowTiming(windowInfo);

    mfd::runtime_api::OffscreenSurface mainSurface;
    mfd::runtime_api::OffscreenSurface previewSurface;
    PresentedTexture mainTexture;
    PresentedTexture previewTexture;

    std::string runtimeError;
    std::cout << "offscreen_viewer\n";
    std::cout << "Window JSON: " << options.windowFile.string() << '\n';
    std::cout << "UDP in/out stays active through mfd_runtime_api.\n";
    std::cout << "Runtime command batches: "
              << (options.noSnapshot ? "non-transactional (--no-snapshot)" : "transactional") << '\n';
    std::cout << "The resizable host window displays two independent offscreen images.\n";

    while (!WindowShouldClose())
    {
        const ViewerLayout layout = BuildViewerLayout(GetScreenWidth(), GetScreenHeight());
        runtimeError.clear();

        try
        {
            if (IsKeyPressed(KEY_R))
            {
                if (!runtimeSession.Reload(error))
                {
                    runtimeError = error;
                }
                else
                {
                    ApplyLoadedWindowDefinition(runtimeSession);
                }
            }

            HandlePageShortcuts(runtimeSession);
            if (!mainSurface.Resize(layout.mainTextureWidth, layout.mainTextureHeight))
            {
                runtimeError = "Unable to resize the primary offscreen surface.";
            }
            if (!previewSurface.Resize(layout.previewTextureWidth, layout.previewTextureHeight))
            {
                runtimeError = runtimeError.empty() ? "Unable to resize the secondary offscreen surface."
                                                    : runtimeError;
            }
            runtimeSession.Advance(GetFrameTime());
            if (!mainSurface.Render(runtimeSession))
            {
                runtimeError = runtimeError.empty() ? "Primary offscreen render failed." : runtimeError;
            }
            if (!previewSurface.Render(runtimeSession))
            {
                runtimeError = runtimeError.empty() ? "Secondary offscreen render failed." : runtimeError;
            }
            if (!UpdatePresentedTexture(mainTexture, mainSurface.FrameView()))
            {
                runtimeError = runtimeError.empty() ? "Primary texture upload failed." : runtimeError;
            }
            if (!UpdatePresentedTexture(previewTexture, previewSurface.FrameView()))
            {
                runtimeError = runtimeError.empty() ? "Secondary texture upload failed." : runtimeError;
            }
        }
        catch (const std::exception& exception)
        {
            runtimeError = exception.what();
        }
        catch (...)
        {
            runtimeError = "Unknown exception while updating the offscreen runtime.";
        }

        BeginDrawing();
        ClearBackground(Color {8, 12, 18, 255});
        DrawSurfacePanel(layout.mainBounds, "Primary offscreen image", mainTexture);
        DrawSurfacePanel(layout.previewBounds, "Secondary offscreen image", previewTexture);
        DrawSidebar(layout.sidebarBounds, runtimeSession, mainSurface, previewSurface, runtimeError);
        EndDrawing();

        if (runtimeSession.WindowInfo().targetFps <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds {1});
        }
    }

    ReleasePresentedTexture(mainTexture);
    ReleasePresentedTexture(previewTexture);
    CloseWindow();
    return 0;
}
} // namespace

int main(const int argc, char** argv)
{
    try
    {
        return MainImpl(argc, argv);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "offscreen_viewer error: " << exception.what() << '\n';
        return 1;
    }
}
