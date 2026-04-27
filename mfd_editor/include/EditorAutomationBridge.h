/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Internal bridge used by automation services to access the live editor state safely.
 */

#include <filesystem>
#include <string>

#include "EditorAutomationTypes.h"

class EditorApplication;

namespace editor::automation
{
/**
 * @brief Internal bridge exposing the editor state to automation services without leaking ImGui details.
 *
 * @details Tests may implement this bridge with a lightweight fake. The
 * concrete editor bridge is responsible for mapping automation-oriented
 * selection and snapshot operations onto `EditorApplication`.
 */
class IEditorAutomationEditorBridge
{
public:
    virtual ~IEditorAutomationEditorBridge() = default;

    /** @brief Returns whether the editor currently owns one loaded window document. */
    [[nodiscard]] virtual bool HasOpenWindow() const noexcept = 0;
    /** @brief Returns the live loaded window configuration. */
    [[nodiscard]] virtual const mfd::LoadedWindowConfiguration& Loaded() const noexcept = 0;
    /** @brief Returns the mutable live loaded window configuration. */
    [[nodiscard]] virtual mfd::LoadedWindowConfiguration& MutableLoaded() noexcept = 0;
    /** @brief Returns the current authored-file layout tracked by the editor. */
    [[nodiscard]] virtual const editor::EditorFileLayout& Files() const noexcept = 0;
    /** @brief Returns the mutable authored-file layout tracked by the editor. */
    [[nodiscard]] virtual editor::EditorFileLayout& MutableFiles() noexcept = 0;
    /** @brief Returns the root authored window file currently associated with the editor. */
    [[nodiscard]] virtual std::filesystem::path WindowFile() const = 0;
    /** @brief Updates the root authored window file currently associated with the editor. */
    virtual void SetWindowFile(std::filesystem::path path) = 0;
    /** @brief Captures one full automation snapshot used for preview sessions and rollback. */
    [[nodiscard]] virtual EditorStateSnapshot CaptureSnapshot() const = 0;
    /** @brief Restores one full automation snapshot previously captured by this bridge. */
    virtual void RestoreSnapshot(const EditorStateSnapshot& snapshot) = 0;
    /** @brief Pushes one snapshot into the editor undo history as one coherent undo step. */
    virtual void PushUndoSnapshot(const EditorStateSnapshot& snapshot) = 0;
    /** @brief Returns the automation-oriented UI selection and camera state. */
    [[nodiscard]] virtual AutomationUiState CaptureUiState() const = 0;
    /** @brief Selects one page by index. */
    virtual void SelectPage(int pageIndex) = 0;
    /** @brief Selects one page reticle by index. */
    virtual void SelectPageReticle(int pageIndex, int reticleIndex) = 0;
    /** @brief Toggles one page reticle inside the current multi-selection. */
    virtual void TogglePageReticleSelection(int pageIndex, int reticleIndex) = 0;
    /** @brief Selects one reticle template in the library. */
    virtual void SelectReticleAsset(std::string templateId) = 0;
    /** @brief Selects one primitive inside one reticle template. */
    virtual void SelectReticlePrimitive(std::string templateId, int primitiveIndex) = 0;
};

/**
 * @brief Concrete bridge mapping automation operations onto one live `EditorApplication`.
 */
class EditorApplicationAutomationBridge final : public IEditorAutomationEditorBridge
{
public:
    /**
     * @brief Binds the bridge to one live editor instance.
     * @param application Editor application serving as the automation owner.
     */
    explicit EditorApplicationAutomationBridge(::EditorApplication& application);

    [[nodiscard]] bool HasOpenWindow() const noexcept override;
    [[nodiscard]] const mfd::LoadedWindowConfiguration& Loaded() const noexcept override;
    [[nodiscard]] mfd::LoadedWindowConfiguration& MutableLoaded() noexcept override;
    [[nodiscard]] const editor::EditorFileLayout& Files() const noexcept override;
    [[nodiscard]] editor::EditorFileLayout& MutableFiles() noexcept override;
    [[nodiscard]] std::filesystem::path WindowFile() const override;
    void SetWindowFile(std::filesystem::path path) override;
    [[nodiscard]] EditorStateSnapshot CaptureSnapshot() const override;
    void RestoreSnapshot(const EditorStateSnapshot& snapshot) override;
    void PushUndoSnapshot(const EditorStateSnapshot& snapshot) override;
    [[nodiscard]] AutomationUiState CaptureUiState() const override;
    void SelectPage(int pageIndex) override;
    void SelectPageReticle(int pageIndex, int reticleIndex) override;
    void TogglePageReticleSelection(int pageIndex, int reticleIndex) override;
    void SelectReticleAsset(std::string templateId) override;
    void SelectReticlePrimitive(std::string templateId, int primitiveIndex) override;

private:
    ::EditorApplication& application_;
};
} // namespace editor::automation
