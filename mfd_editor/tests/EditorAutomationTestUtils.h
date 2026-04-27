/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared GoogleTest helpers for editor automation facade and plugin loader scenarios.
 */

#include <filesystem>
#include <string>
#include <vector>

#include "EditorAutomationBridge.h"
#include "EditorAutomationServices.h"

/**
 * @brief Fake editor bridge used by editor automation tests.
 */
class FakeEditorAutomationBridge final : public editor::automation::IEditorAutomationEditorBridge
{
public:
    bool HasOpenWindow() const noexcept override;
    const mfd::LoadedWindowConfiguration& Loaded() const noexcept override;
    mfd::LoadedWindowConfiguration& MutableLoaded() noexcept override;
    const editor::EditorFileLayout& Files() const noexcept override;
    editor::EditorFileLayout& MutableFiles() noexcept override;
    std::filesystem::path WindowFile() const override;
    void SetWindowFile(std::filesystem::path path) override;
    editor::automation::EditorStateSnapshot CaptureSnapshot() const override;
    void RestoreSnapshot(const editor::automation::EditorStateSnapshot& snapshot) override;
    void PushUndoSnapshot(const editor::automation::EditorStateSnapshot& snapshot) override;
    editor::automation::AutomationUiState CaptureUiState() const override;
    void SelectPage(int pageIndex) override;
    void SelectPageReticle(int pageIndex, int reticleIndex) override;
    void TogglePageReticleSelection(int pageIndex, int reticleIndex) override;
    void SelectReticleAsset(std::string templateId) override;
    void SelectReticlePrimitive(std::string templateId, int primitiveIndex) override;

    std::filesystem::path windowFile_ {};
    mfd::LoadedWindowConfiguration loaded_ {};
    editor::EditorFileLayout files_ {};
    editor::automation::AutomationUiState uiState_ {};
    std::vector<editor::automation::EditorStateSnapshot> pushedUndoSnapshots_ {};
};

/**
 * @brief Creates one temporary folder unique to the current test run.
 * @return Fresh writable folder.
 */
std::filesystem::path MakeEditorAutomationTemporaryFolder();

/**
 * @brief Creates one bridge pre-seeded with one page and one reticle asset.
 * @return Ready-to-use fake bridge.
 */
FakeEditorAutomationBridge MakeSeedEditorAutomationBridge();
