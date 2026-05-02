/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Windows-native file-dialog helpers kept out of the main editor shell implementation.
 */

#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "EditorFileDialogs.h"

#include <array>

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace editor
{
namespace
{
std::optional<std::filesystem::path> OpenJsonAssetFileDialog(const [[maybe_unused]] std::filesystem::path& initialFolder,
                                                             const wchar_t* dialogTitle,
                                                             std::string* error)
{
#if defined(_WIN32)
    static constexpr wchar_t kJsonAssetFilter[] = L"JSON Assets (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    std::wstring initialFolderWide = initialFolder.wstring();
    std::array<wchar_t, 4096> selectedFileBuffer {};

    OPENFILENAMEW dialog {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = kJsonAssetFilter;
    dialog.lpstrFile = selectedFileBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(selectedFileBuffer.size());
    dialog.lpstrInitialDir = initialFolderWide.c_str();
    dialog.lpstrTitle = dialogTitle;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog))
    {
        const DWORD dialogError = CommDlgExtendedError();
        if (dialogError != 0 && error != nullptr)
        {
            *error = "Opening the JSON asset picker failed.";
        }
        return std::nullopt;
    }

    return std::filesystem::path(dialog.lpstrFile);
#else
    if (error != nullptr)
    {
        *error = "Native file explorer integration is only available on Windows in this build.";
    }
    return std::nullopt;
#endif
}
} // namespace

std::optional<std::filesystem::path> OpenWindowAssetFileDialog(const std::filesystem::path& initialFolder,
                                                               std::string* error)
{
    return OpenJsonAssetFileDialog(initialFolder, L"Open MFD window asset", error);
}

std::optional<std::filesystem::path> OpenPageAssetFileDialog(const std::filesystem::path& initialFolder,
                                                             std::string* error)
{
    return OpenJsonAssetFileDialog(initialFolder, L"Import MFD page asset", error);
}
} // namespace editor
