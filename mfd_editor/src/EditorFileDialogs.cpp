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

#include <algorithm>
#include <array>
#include <string_view>

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>
#endif

namespace editor
{
namespace
{
class ScopedComInitialization
{
public:
    ScopedComInitialization()
    {
#if defined(_WIN32)
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        initialized_ = result == S_OK || result == S_FALSE;
        ready_ = initialized_ || result == RPC_E_CHANGED_MODE;
#endif
    }

    ~ScopedComInitialization()
    {
#if defined(_WIN32)
        if (initialized_)
        {
            CoUninitialize();
        }
#endif
    }

    [[nodiscard]] bool Ready() const noexcept
    {
        return ready_;
    }

private:
    bool initialized_ = false;
    bool ready_ = false;
};

std::wstring ToWideString(const std::string_view text)
{
    return std::wstring(text.begin(), text.end());
}

std::optional<std::filesystem::path> OpenExistingFileDialog(const [[maybe_unused]] std::filesystem::path& initialFolder,
                                                            [[maybe_unused]] const wchar_t* dialogTitle,
                                                            [[maybe_unused]] const wchar_t* filter,
                                                            std::string* error)
{
#if defined(_WIN32)
    std::wstring initialFolderWide = initialFolder.wstring();
    std::array<wchar_t, 4096> selectedFileBuffer {};

    OPENFILENAMEW dialog {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = filter;
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
            *error = "Opening the native file picker failed.";
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

std::optional<std::filesystem::path> OpenJsonAssetFileDialog(const std::filesystem::path& initialFolder,
                                                             const wchar_t* dialogTitle,
                                                             std::string* error)
{
    static constexpr wchar_t kJsonAssetFilter[] = L"JSON Assets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    return OpenExistingFileDialog(initialFolder, dialogTitle, kJsonAssetFilter, error);
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

std::optional<std::filesystem::path> SaveJsonAssetFileDialog([[maybe_unused]] const std::filesystem::path& suggestedFile,
                                                             [[maybe_unused]] const std::string_view title,
                                                             std::string* error)
{
#if defined(_WIN32)
    static constexpr wchar_t kJsonAssetFilter[] = L"JSON Assets (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    const std::filesystem::path normalizedSuggestedFile =
        suggestedFile.empty() ? std::filesystem::path("new_asset.json") : suggestedFile.lexically_normal();
    const std::filesystem::path initialFolder =
        normalizedSuggestedFile.has_parent_path()
            ? normalizedSuggestedFile.parent_path()
            : std::filesystem::current_path();
    const std::wstring initialFolderWide = initialFolder.wstring();

    std::array<wchar_t, 4096> selectedFileBuffer {};
    const std::wstring fileNameWide = normalizedSuggestedFile.filename().wstring();
    const std::size_t fileNameSize = std::min(fileNameWide.size(), selectedFileBuffer.size() - 1U);
    std::copy_n(fileNameWide.c_str(), fileNameSize, selectedFileBuffer.data());
    selectedFileBuffer[fileNameSize] = L'\0';

    const std::wstring titleWide = ToWideString(title);
    OPENFILENAMEW dialog {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFilter = kJsonAssetFilter;
    dialog.lpstrFile = selectedFileBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(selectedFileBuffer.size());
    dialog.lpstrInitialDir = initialFolderWide.c_str();
    dialog.lpstrTitle = titleWide.c_str();
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_EXPLORER;

    if (!GetSaveFileNameW(&dialog))
    {
        const DWORD dialogError = CommDlgExtendedError();
        if (dialogError != 0 && error != nullptr)
        {
            *error = "Opening the native JSON save picker failed.";
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

std::optional<std::filesystem::path> OpenFontAssetFileDialog(const std::filesystem::path& initialFolder,
                                                             std::string* error)
{
    static constexpr wchar_t kFontAssetFilter[] =
        L"Font Assets (*.ttf;*.otf)\0*.ttf;*.otf\0All Files (*.*)\0*.*\0";
    return OpenExistingFileDialog(initialFolder, L"Select MFD window font", kFontAssetFilter, error);
}

std::optional<std::filesystem::path> OpenFolderDialog(const std::filesystem::path& initialFolder,
                                                      const std::string_view title,
                                                      std::string* error)
{
#if defined(_WIN32)
    ScopedComInitialization comInitialization;
    if (!comInitialization.Ready())
    {
        if (error != nullptr)
        {
            *error = "Initializing the native folder picker failed.";
        }
        return std::nullopt;
    }

    IFileOpenDialog* dialog = nullptr;
    const HRESULT createResult =
        CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(createResult) || dialog == nullptr)
    {
        if (error != nullptr)
        {
            *error = "Creating the native folder picker failed.";
        }
        return std::nullopt;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);

    const std::wstring titleWide = ToWideString(title);
    dialog->SetTitle(titleWide.c_str());

    const std::filesystem::path normalizedInitialFolder =
        initialFolder.empty() ? std::filesystem::current_path() : std::filesystem::absolute(initialFolder).lexically_normal();
    if (!normalizedInitialFolder.empty())
    {
        IShellItem* initialItem = nullptr;
        const HRESULT initialFolderResult =
            SHCreateItemFromParsingName(normalizedInitialFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&initialItem));
        if (SUCCEEDED(initialFolderResult) && initialItem != nullptr)
        {
            dialog->SetDefaultFolder(initialItem);
            dialog->SetFolder(initialItem);
            initialItem->Release();
        }
    }

    const HRESULT showResult = dialog->Show(GetActiveWindow());
    if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        dialog->Release();
        return std::nullopt;
    }

    if (FAILED(showResult))
    {
        if (error != nullptr)
        {
            *error = "Opening the folder picker failed.";
        }
        dialog->Release();
        return std::nullopt;
    }

    IShellItem* resultItem = nullptr;
    const HRESULT resultItemStatus = dialog->GetResult(&resultItem);
    dialog->Release();
    if (FAILED(resultItemStatus) || resultItem == nullptr)
    {
        if (error != nullptr)
        {
            *error = "Reading the selected folder failed.";
        }
        return std::nullopt;
    }

    PWSTR folderPathWide = nullptr;
    const HRESULT displayNameStatus = resultItem->GetDisplayName(SIGDN_FILESYSPATH, &folderPathWide);
    resultItem->Release();
    if (FAILED(displayNameStatus) || folderPathWide == nullptr)
    {
        if (error != nullptr)
        {
            *error = "Reading the selected folder path failed.";
        }
        return std::nullopt;
    }

    std::filesystem::path selectedFolder(folderPathWide);
    CoTaskMemFree(folderPathWide);
    return selectedFolder;
#else
    if (error != nullptr)
    {
        *error = "Native folder selection is only available on Windows in this build.";
    }
    return std::nullopt;
#endif
}

bool OpenFolderInFileExplorer(const std::filesystem::path& folder, std::string* error)
{
#if defined(_WIN32)
    if (folder.empty())
    {
        if (error != nullptr)
        {
            *error = "No folder was provided to the native file explorer.";
        }
        return false;
    }

    const std::filesystem::path normalizedFolder = std::filesystem::absolute(folder).lexically_normal();
    const HINSTANCE result = ShellExecuteW(GetActiveWindow(),
                                           L"open",
                                           normalizedFolder.wstring().c_str(),
                                           nullptr,
                                           nullptr,
                                           SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        if (error != nullptr)
        {
            *error = "Opening the export folder in the native file explorer failed.";
        }
        return false;
    }

    return true;
#else
    if (error != nullptr)
    {
        *error = "Native file explorer integration is only available on Windows in this build.";
    }
    return false;
#endif
}
} // namespace editor
