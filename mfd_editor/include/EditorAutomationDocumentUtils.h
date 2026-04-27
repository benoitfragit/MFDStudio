/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Shared helper functions used by editor automation services and tests.
 */

#include <optional>
#include <string_view>

#include "EditorAutomationTypes.h"

namespace editor::automation
{
/** @brief Builds one stable automation window id from the current loaded window state. */
[[nodiscard]] WindowId MakeWindowId(const mfd::LoadedWindowConfiguration& loaded);
/** @brief Builds one stable automation page id from one page definition. */
[[nodiscard]] PageId MakePageId(const mfd::PageDefinition& page);
/** @brief Builds one stable automation reticle-template id from one template id. */
[[nodiscard]] ReticleAssetId MakeReticleAssetId(std::string_view templateId);
/** @brief Builds one stable automation page-reticle-instance id. */
[[nodiscard]] PageReticleInstanceId MakePageReticleInstanceId(const mfd::PageDefinition& page,
                                                              const mfd::ReticleGroup& reticle);
/** @brief Builds one stable automation primitive id for one template-owned primitive. */
[[nodiscard]] PrimitiveId MakeReticleAssetPrimitiveId(std::string_view templateId,
                                                      const mfd::Primitive& primitive,
                                                      int primitiveIndex);
/** @brief Builds one stable automation primitive id for one page-reticle-owned primitive. */
[[nodiscard]] PrimitiveId MakePageReticlePrimitiveId(const mfd::PageDefinition& page,
                                                     const mfd::ReticleGroup& reticle,
                                                     const mfd::Primitive& primitive,
                                                     int primitiveIndex);
/** @brief Builds one stable automation editor-layer id. */
[[nodiscard]] LayerId MakeLayerId(const mfd::PageDefinition& page, const mfd::EditorLayerDefinition& layer);

/** @brief Locates one page from one automation page id. */
[[nodiscard]] mfd::PageDefinition* FindPageById(mfd::LoadedWindowConfiguration& loaded,
                                                const PageId& pageId,
                                                int* pageIndex = nullptr);
/** @brief Locates one page from one automation page id. */
[[nodiscard]] const mfd::PageDefinition* FindPageById(const mfd::LoadedWindowConfiguration& loaded,
                                                      const PageId& pageId,
                                                      int* pageIndex = nullptr);
/** @brief Locates one reticle template from one automation reticle-asset id. */
[[nodiscard]] mfd::ReticleGroup* FindReticleAssetById(mfd::LoadedWindowConfiguration& loaded, const ReticleAssetId& reticleId);
/** @brief Locates one reticle template from one automation reticle-asset id. */
[[nodiscard]] const mfd::ReticleGroup* FindReticleAssetById(const mfd::LoadedWindowConfiguration& loaded,
                                                            const ReticleAssetId& reticleId);
/** @brief Locates one page-reticle instance from one automation page-reticle id. */
[[nodiscard]] mfd::ReticleGroup* FindPageReticleById(mfd::LoadedWindowConfiguration& loaded,
                                                     const PageReticleInstanceId& reticleId,
                                                     mfd::PageDefinition** page = nullptr,
                                                     int* pageIndex = nullptr,
                                                     int* reticleIndex = nullptr);
/** @brief Locates one page-reticle instance from one automation page-reticle id. */
[[nodiscard]] const mfd::ReticleGroup* FindPageReticleById(const mfd::LoadedWindowConfiguration& loaded,
                                                           const PageReticleInstanceId& reticleId,
                                                           const mfd::PageDefinition** page = nullptr,
                                                           int* pageIndex = nullptr,
                                                           int* reticleIndex = nullptr);
/** @brief Locates one editor layer from one automation layer id. */
[[nodiscard]] mfd::EditorLayerDefinition* FindLayerById(mfd::LoadedWindowConfiguration& loaded,
                                                        const PageId& pageId,
                                                        const LayerId& layerId,
                                                        int* layerIndex = nullptr);
/** @brief Locates one editor layer from one automation layer id. */
[[nodiscard]] const mfd::EditorLayerDefinition* FindLayerById(const mfd::LoadedWindowConfiguration& loaded,
                                                              const PageId& pageId,
                                                              const LayerId& layerId,
                                                              int* layerIndex = nullptr);
/** @brief Ensures that every page owns at least one editor-visible layer. */
void BootstrapEditorLayersForPage(mfd::PageDefinition& page);
/** @brief Returns the default editor layer assigned to newly created page reticles. */
[[nodiscard]] std::string DefaultEditorLayerId(const mfd::PageDefinition& page);
/** @brief Generates one unique page-reticle id inside one page. */
[[nodiscard]] std::string MakeUniqueReticleId(const std::vector<mfd::ReticleGroup>& groups, std::string_view baseId);
/** @brief Builds one seed reticle containing one primitive of the requested type. */
[[nodiscard]] mfd::ReticleGroup MakePrimitiveReticle(std::string id, mfd::PrimitiveType primitiveType);
/** @brief Builds one automation snapshot from the live editor model and file layout. */
[[nodiscard]] DocumentSnapshot BuildDocumentSnapshot(const mfd::LoadedWindowConfiguration& loaded,
                                                     const editor::EditorFileLayout& files,
                                                     const AutomationUiState& uiState,
                                                     const std::optional<AutomationSessionId>& activeSessionId);
/** @brief Validates the live editor model for automation usage. */
[[nodiscard]] ValidationReport ValidateAutomationState(const mfd::LoadedWindowConfiguration& loaded,
                                                       const editor::EditorFileLayout& files);
/** @brief Returns `true` when the path targets one staged `_Exec` output tree instead of source assets. */
[[nodiscard]] bool IsExecStagingPath(const std::filesystem::path& path);
/** @brief Resolves one primitive index/id pair against one primitive container. */
[[nodiscard]] int ResolvePrimitiveIndex(const std::vector<mfd::Primitive>& primitives, const PrimitiveSelector& primitiveSelector);
} // namespace editor::automation
