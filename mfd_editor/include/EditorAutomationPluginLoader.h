/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Hidden loader used by the editor and tests to host one stable-ABI automation plugin DLL in-process.
 */

#include <filesystem>
#include <memory>
#include <string>

#include "EditorAutomationServices.h"
#include "mfd/editor/AutomationPlugin.h"

namespace editor
{
/**
 * @brief Loads, starts, ticks and unloads one stable-ABI in-process editor automation plugin DLL.
 */
class EditorAutomationPluginLoader
{
public:
    EditorAutomationPluginLoader();
    ~EditorAutomationPluginLoader() noexcept;

    EditorAutomationPluginLoader(const EditorAutomationPluginLoader&) = delete;
    EditorAutomationPluginLoader& operator=(const EditorAutomationPluginLoader&) = delete;

    /**
     * @brief Loads one plugin DLL and starts it against the provided automation facade.
     * @param pluginFile Plugin DLL file to load.
     * @param facade Live automation facade owned by the editor.
     * @param error Receives one human-readable error on failure.
     * @return `true` on success, otherwise `false`.
     */
    [[nodiscard]] bool Load(const std::filesystem::path& pluginFile,
                            editor::automation::IEditorAutomationFacade& facade,
                            std::string& error);
    /** @brief Ticks the currently loaded plugin once, if one is active. */
    void Tick() noexcept;
    /** @brief Stops and unloads the currently loaded plugin. */
    void Unload() noexcept;
    /** @brief Returns whether one plugin is currently loaded. */
    [[nodiscard]] bool IsLoaded() const noexcept;
    /** @brief Returns the currently loaded plugin display name, or an empty string when none is loaded. */
    [[nodiscard]] std::string PluginDisplayName() const;
    /** @brief Returns the plugin file currently loaded by this loader. */
    [[nodiscard]] std::filesystem::path PluginFile() const;
    /** @brief Returns the last runtime error captured while ticking the plugin. */
    [[nodiscard]] const std::string& LastRuntimeError() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_ {};
};
} // namespace editor
