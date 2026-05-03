/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Global reticle-template rename planning helpers used by the editor shell to update shared page assets safely.
 */

#include <filesystem>
#include <string>
#include <vector>

#include "AssetReferenceIndexService.h"
#include "EditorDocumentSerializer.h"
#include "mfd/io/JsonLoader.h"

namespace editor
{
/**
 * @brief Input used to plan one global reticle-template rename across the scanned asset tree.
 */
struct RenameReticleRequest
{
    /** @brief Existing reticle-template id selected in the current editor document. */
    std::string oldTemplateId {};
    /** @brief Requested reticle-template id after the rename. */
    std::string newTemplateId {};
    /** @brief Asset root scanned for page references and template collisions. */
    std::filesystem::path assetsRoot {};
    /** @brief Renames the backing template JSON file in addition to the logical template id when enabled. */
    bool renameTemplateFile = false;
};

/**
 * @brief One exact page-level reference that would be rewritten by the reticle rename.
 */
struct RenameReticleReference
{
    /** @brief Semantic kind of page-level reference. */
    ReticleReferenceKind kind = ReticleReferenceKind::PageReticleTemplate;
    /** @brief Page JSON file owning the reference. */
    std::filesystem::path pageFile {};
    /** @brief Authored page name owning the reference. */
    std::string pageName {};
    /** @brief Page reticle or strobe id owning the reference when available. */
    std::string ownerReticleId {};
};

/**
 * @brief One blocking page-level collision discovered while validating the rename.
 */
struct RenameReticleCollision
{
    /** @brief Page JSON file that already references the requested target template id. */
    std::filesystem::path pageFile {};
    /** @brief Authored page name owning the conflict. */
    std::string pageName {};
    /** @brief Semantic kind of conflicting page-level reference. */
    ReticleReferenceKind kind = ReticleReferenceKind::PageReticleTemplate;
    /** @brief Page reticle or strobe id that already uses the requested target template id. */
    std::string conflictingReticleId {};
    /** @brief Conflicting template id already present on that page. */
    std::string conflictingTemplateId {};
};

/**
 * @brief Pure plan describing whether a global reticle-template rename can execute safely.
 */
struct RenameReticlePlan
{
    /** @brief Indicates whether the plan is valid and executable. */
    bool canExecute = false;
    /** @brief Current logical reticle-template id before the rename. */
    std::string oldReticleName {};
    /** @brief Requested logical reticle-template id after the rename. */
    std::string newReticleName {};
    /** @brief Current source template JSON file. */
    std::filesystem::path currentTemplateFile {};
    /** @brief Target source template JSON file after the optional file rename. */
    std::filesystem::path targetTemplateFile {};
    /** @brief Indicates whether the backing template JSON file should be renamed too. */
    bool renameTemplateFile = false;
    /** @brief Asset root scanned for references and collisions. */
    std::filesystem::path assetsRoot {};
    /** @brief Exact page-level references discovered for the target template id. */
    std::vector<RenameReticleReference> references {};
    /** @brief Blocking page-level collisions discovered while evaluating the rename. */
    std::vector<RenameReticleCollision> collisions {};
    /** @brief Concrete JSON files that would be rewritten by `Execute()`. */
    std::vector<std::filesystem::path> filesToModify {};
    /** @brief Files that would be deleted by `Execute()`. */
    std::vector<std::filesystem::path> filesToDelete {};
    /** @brief Non-blocking warnings shown to the user before execution. */
    std::vector<std::string> warnings {};
    /** @brief Validation error explaining why the plan cannot execute. */
    std::string error {};
};

/**
 * @brief Mutable execution result returned once one valid global reticle-template rename has been applied.
 */
struct RenameReticleResult
{
    /** @brief JSON files rewritten on disk by the rename. */
    std::vector<std::filesystem::path> modifiedFiles {};
    /** @brief Files deleted on disk by the rename. */
    std::vector<std::filesystem::path> deletedFiles {};
    /** @brief Number of distinct page JSON files rewritten. */
    std::size_t updatedPageCount = 0U;
    /** @brief Indicates whether the backing template JSON file was moved to a new path. */
    bool renamedTemplateFile = false;
    /** @brief Indicates whether the current in-memory editor document was resynchronized. */
    bool updatedCurrentDocument = false;
};

/**
 * @brief Stateless service planning and applying global reticle-template renames without embedding filesystem logic in ImGui.
 */
class ReticleRenameService
{
public:
    /**
     * @brief Builds one read-only plan for renaming a shared reticle template across the scanned page tree.
     * @param loaded Current authored window and document state.
     * @param files Current authored file layout tracked by the editor.
     * @param request Requested rename inputs.
     * @return Pure plan describing whether the rename can execute safely.
     */
    [[nodiscard]] RenameReticlePlan BuildPlan(const mfd::LoadedWindowConfiguration& loaded,
                                              const EditorFileLayout& files,
                                              const RenameReticleRequest& request) const;

    /**
     * @brief Applies one valid global reticle-template rename to disk and resynchronizes the current editor document.
     * @param plan Plan produced by `BuildPlan()`.
     * @param loaded Mutable authored window/document state.
     * @param files Mutable file layout mirrored by the editor.
     * @param result Optional output populated with modified files and synchronization details.
     * @param error Optional output populated when execution fails.
     * @return `true` when the rename was applied.
     */
    bool Execute(const RenameReticlePlan& plan,
                 mfd::LoadedWindowConfiguration& loaded,
                 EditorFileLayout& files,
                 RenameReticleResult* result,
                 std::string* error) const;

private:
    AssetReferenceIndexService referenceIndexService_ {};
};
} // namespace editor
