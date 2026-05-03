/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Design-document export helpers used by the editor to generate Markdown ICD files and exploded views.
 */

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor
{
/**
 * @brief Input used to stage one design-documentation export.
 */
struct DesignExportRequest
{
    /** @brief Folder selected by the user for the export. */
    std::filesystem::path outputFolder;
    /** @brief Source window JSON file shown in the generated documentation. */
    std::filesystem::path windowFile;
    /** @brief Loaded in-memory window and document state to export. */
    const mfd::LoadedWindowConfiguration* loaded = nullptr;
    /** @brief Authored file layout used to map page indices back to source JSON files. */
    const EditorFileLayout* files = nullptr;
    /** @brief Enables Markdown document generation. */
    bool exportMarkdownIcd = true;
    /** @brief Enables exploded-view image generation. */
    bool exportExplodedViews = true;
    /** @brief Includes authored canvas coordinates in labels and Markdown tables. */
    bool includeCanvasCoordinates = true;
    /** @brief Includes generic C++ command snippets derived from authored names. */
    bool includeCppSnippets = true;
    /** @brief Includes the strobe section when the page defines one. */
    bool includeStrobe = true;
    /** @brief Includes the blink-type section when the page defines named blink types. */
    bool includeBlink = true;
    /** @brief Includes primitive ids and exposed transport ids when available. */
    bool includePrimitiveIds = true;
    /** @brief Includes the generated mapping hash when a companion transport map is available. */
    bool includeMappingHash = true;
};

/**
 * @brief Per-page artifact paths staged by one export plan.
 */
struct DesignExportPageArtifact
{
    /** @brief Zero-based page index in the loaded document. */
    std::size_t pageIndex = 0;
    /** @brief Authored page name. */
    std::string pageName;
    /** @brief Source page JSON file when known. */
    std::filesystem::path sourcePageFile;
    /** @brief Safe file stem derived from the page name. */
    std::string safeFileStem;
    /** @brief Markdown ICD file generated for this page. */
    std::filesystem::path markdownFile;
    /** @brief Exploded-view image generated for this page, when enabled. */
    std::filesystem::path imageFile;
};

/**
 * @brief Pure plan describing the files and data used by one design export.
 */
struct DesignExportPlan
{
    /** @brief Indicates whether the export can execute. */
    bool canExecute = false;
    /** @brief Final output folder used by the export. */
    std::filesystem::path outputFolder;
    /** @brief Source window JSON file shown in the generated documentation. */
    std::filesystem::path windowFile;
    /** @brief Files expected to be generated inside the export folder. */
    std::vector<std::filesystem::path> filesToWrite;
    /** @brief Non-fatal warnings reported before or during execution. */
    std::vector<std::string> warnings;
    /** @brief Blocking error describing why the export cannot execute. */
    std::string error;
    /** @brief Indicates whether Markdown documents should be generated. */
    bool exportMarkdownIcd = true;
    /** @brief Indicates whether exploded-view images should be generated. */
    bool exportExplodedViews = true;
    /** @brief Indicates whether authored canvas coordinates should be documented. */
    bool includeCanvasCoordinates = true;
    /** @brief Indicates whether generic C++ command snippets should be generated. */
    bool includeCppSnippets = true;
    /** @brief Indicates whether the strobe section should be generated. */
    bool includeStrobe = true;
    /** @brief Indicates whether the blink section should be generated. */
    bool includeBlink = true;
    /** @brief Indicates whether primitive ids should be listed. */
    bool includePrimitiveIds = true;
    /** @brief Indicates whether the mapping hash should be shown when available. */
    bool includeMappingHash = true;
    /** @brief Planned README file. */
    std::filesystem::path readmeFile;
    /** @brief Planned window ICD file. */
    std::filesystem::path windowIcdFile;
    /** @brief Planned manifest file. */
    std::filesystem::path manifestFile;
    /** @brief Planned per-page artifacts. */
    std::vector<DesignExportPageArtifact> pages;
    /** @brief Snapshot of the loaded document exported by this plan. */
    mfd::LoadedWindowConfiguration loaded {};
    /** @brief Snapshot of the authored file layout exported by this plan. */
    EditorFileLayout files {};
};

/**
 * @brief Result returned after one design export writes files to disk.
 */
struct DesignExportResult
{
    /** @brief Final output folder created by the export. */
    std::filesystem::path outputFolder;
    /** @brief Files successfully written by the export. */
    std::vector<std::filesystem::path> writtenFiles;
    /** @brief Non-fatal warnings collected during execution. */
    std::vector<std::string> warnings;
};

/**
 * @brief Stateless service generating design documentation and exploded preview images for the current editor window.
 */
class EditorDesignExportService
{
public:
    /**
     * @brief Optional callback used to override exploded-view rendering during tests.
     */
    using ExplodedViewRenderer =
        std::function<bool(const DesignExportPlan&, const DesignExportPageArtifact&, std::string* error)>;

    /**
     * @brief Builds the service with an optional exploded-view renderer override.
     * @param explodedViewRenderer Optional callback used instead of the default raylib renderer.
     */
    explicit EditorDesignExportService(ExplodedViewRenderer explodedViewRenderer = {});

    /**
     * @brief Builds one read-only export plan from the current editor state.
     * @param request Export inputs selected by the user.
     * @return Pure plan describing the target files and warnings.
     */
    [[nodiscard]] DesignExportPlan BuildPlan(const DesignExportRequest& request) const;

    /**
     * @brief Executes one valid design export plan and writes files to disk.
     * @param plan Plan produced by `BuildPlan()`.
     * @return Result listing the files that were written successfully.
     */
    [[nodiscard]] DesignExportResult Execute(const DesignExportPlan& plan) const;

private:
    [[nodiscard]] bool RenderExplodedView(const DesignExportPlan& plan,
                                          const DesignExportPageArtifact& artifact,
                                          std::string* error) const;

    ExplodedViewRenderer explodedViewRenderer_ {};
};
} // namespace editor
