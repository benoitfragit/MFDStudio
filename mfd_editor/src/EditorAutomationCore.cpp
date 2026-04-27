/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Core automation services used by the hidden external editor automation host.
 */

#include "EditorAutomationServices.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

#include "EditorAutomationDocumentUtils.h"

namespace editor
{
std::string SerializePageToJsonString(const mfd::PageDefinition& page,
                                      const mfd::ReticleLibrary& library,
                                      const EditorFileLayout& layout,
                                      std::size_t pageIndex);
std::string SerializeWindowToJsonString(const mfd::WindowAssetDefinition& window,
                                        const mfd::MfdDocument& document,
                                        const EditorFileLayout& layout);
} // namespace editor

namespace editor::automation
{
namespace
{
AutomationError MakeError(const AutomationErrorCode code, std::string message)
{
    return AutomationError {code, std::move(message)};
}

AutomationStatus FailureStatus(const AutomationErrorCode code, std::string message)
{
    AutomationStatus status;
    status.error = MakeError(code, std::move(message));
    return status;
}

template <typename T>
AutomationResult<T> FailureResult(const AutomationErrorCode code, std::string message)
{
    AutomationResult<T> result;
    result.error = MakeError(code, std::move(message));
    return result;
}

std::filesystem::path ResolveAgainst(const std::filesystem::path& anchorFile, const std::filesystem::path& candidate)
{
    if (candidate.empty())
    {
        return {};
    }

    if (candidate.is_absolute())
    {
        return candidate.lexically_normal();
    }

    if (anchorFile.empty())
    {
        return candidate.lexically_normal();
    }

    return (anchorFile.parent_path() / candidate).lexically_normal();
}

std::filesystem::path DefaultReticleLibraryFolder(const std::filesystem::path& windowFile)
{
    if (windowFile.empty())
    {
        return {};
    }

    return (windowFile.parent_path() / "reticles").lexically_normal();
}

struct PrimitiveOwnerAccess
{
    std::vector<mfd::Primitive>* primitives = nullptr;
    std::string ownerId {};
};

PrimitiveOwnerAccess ResolvePrimitiveOwner(mfd::LoadedWindowConfiguration& loaded,
                                          const AutomationPrimitiveOwnerKind ownerKind,
                                          const std::string_view ownerId)
{
    PrimitiveOwnerAccess result;
    result.ownerId = std::string(ownerId);

    if (ownerKind == AutomationPrimitiveOwnerKind::ReticleAsset)
    {
        mfd::ReticleGroup* reticle = FindReticleAssetById(loaded, ReticleAssetId {std::string(ownerId)});
        if (reticle != nullptr)
        {
            result.primitives = &reticle->primitives;
        }
        return result;
    }

    mfd::ReticleGroup* pageReticle = FindPageReticleById(loaded, PageReticleInstanceId {std::string(ownerId)});
    if (pageReticle != nullptr)
    {
        result.primitives = &pageReticle->primitives;
    }
    return result;
}

bool HasDuplicatePrimitiveId(const std::vector<mfd::Primitive>& primitives,
                             const std::string_view candidateId,
                             const int ignoredIndex = -1)
{
    if (candidateId.empty())
    {
        return false;
    }

    for (int index = 0; index < static_cast<int>(primitives.size()); ++index)
    {
        if (index == ignoredIndex)
        {
            continue;
        }

        if (mfd::NormalizePageName(primitives[static_cast<std::size_t>(index)].id) == mfd::NormalizePageName(candidateId))
        {
            return true;
        }
    }

    return false;
}

bool HasDuplicatePageName(const mfd::LoadedWindowConfiguration& loaded, const std::string_view pageName, const int ignoredIndex = -1)
{
    const std::string normalizedPageName = mfd::NormalizePageName(pageName);
    for (int index = 0; index < static_cast<int>(loaded.document.pages.size()); ++index)
    {
        if (index == ignoredIndex)
        {
            continue;
        }

        const mfd::PageDefinition& page = loaded.document.pages[static_cast<std::size_t>(index)];
        if (page.normalizedName == normalizedPageName)
        {
            return true;
        }
    }

    return false;
}

bool HasDuplicateReticleAssetId(const mfd::LoadedWindowConfiguration& loaded,
                                const std::string_view reticleId,
                                const std::string_view ignoredKey = {})
{
    const std::string normalizedReticleId = mfd::NormalizePageName(reticleId);
    for (const auto& [candidateKey, reticle] : loaded.document.reticleLibrary)
    {
        if (!ignoredKey.empty() && mfd::NormalizePageName(candidateKey) == mfd::NormalizePageName(ignoredKey))
        {
            continue;
        }

        if (mfd::NormalizePageName(candidateKey) == normalizedReticleId ||
            mfd::NormalizePageName(reticle.id) == normalizedReticleId)
        {
            return true;
        }
    }

    return false;
}

bool HasDuplicatePageReticleId(const mfd::PageDefinition& page, const std::string_view reticleId, const int ignoredIndex = -1)
{
    const std::string normalizedReticleId = mfd::NormalizePageName(reticleId);
    for (int index = 0; index < static_cast<int>(page.staticReticles.size()); ++index)
    {
        if (index == ignoredIndex)
        {
            continue;
        }

        if (mfd::NormalizePageName(page.staticReticles[static_cast<std::size_t>(index)].id) == normalizedReticleId)
        {
            return true;
        }
    }

    return false;
}

void SyncWindowPageFiles(mfd::LoadedWindowConfiguration& loaded, const editor::EditorFileLayout& files)
{
    loaded.window.pageFiles = files.pageFiles;
}

std::vector<std::filesystem::path> CollectSavedFiles(const mfd::LoadedWindowConfiguration& loaded, const editor::EditorFileLayout& files)
{
    std::vector<std::filesystem::path> savedFiles;
    if (!loaded.window.sourceFile.empty())
    {
        savedFiles.push_back(loaded.window.sourceFile);
    }

    savedFiles.insert(savedFiles.end(), files.pageFiles.begin(), files.pageFiles.end());
    for (const auto& [templateId, templateFile] : files.templateFiles)
    {
        static_cast<void>(templateId);
        savedFiles.push_back(templateFile);
    }

    std::sort(savedFiles.begin(), savedFiles.end());
    savedFiles.erase(std::unique(savedFiles.begin(), savedFiles.end()), savedFiles.end());
    return savedFiles;
}

class EventQueue
{
public:
    void Push(AutomationEventKind kind, std::string entityId, std::string message)
    {
        events_.push_back(AutomationEvent {kind, std::move(entityId), std::move(message)});
    }

    [[nodiscard]] std::vector<AutomationEvent> Consume()
    {
        std::vector<AutomationEvent> result = std::move(events_);
        events_.clear();
        return result;
    }

private:
    std::vector<AutomationEvent> events_ {};
};

struct SessionState
{
    AutomationSessionId id {};
    std::string label {};
    EditorStateSnapshot preSessionSnapshot {};
};

struct AutomationCoreContext
{
    IEditorAutomationEditorBridge& bridge;
    EventQueue events {};
    std::optional<SessionState> activeSession {};
    std::uint64_t nextSessionSerial = 1U;
};

class AutomationQueryService final : public IEditorAutomationQueryService
{
public:
    explicit AutomationQueryService(AutomationCoreContext& context)
        : context_(context)
    {
    }

    [[nodiscard]] AutomationResult<DocumentSnapshot> GetSnapshot() const override
    {
        return AutomationResult<DocumentSnapshot>::Success(
            BuildDocumentSnapshot(context_.bridge.Loaded(),
                                  context_.bridge.Files(),
                                  context_.bridge.CaptureUiState(),
                                  context_.activeSession.has_value() ? std::optional<AutomationSessionId> {context_.activeSession->id}
                                                                     : std::nullopt));
    }

private:
    AutomationCoreContext& context_;
};

class AutomationAuthoringService final : public IEditorAutomationAuthoringService
{
public:
    explicit AutomationAuthoringService(AutomationCoreContext& context)
        : context_(context)
    {
    }

    AutomationStatus ApplyAction(const AutomationAction& action) override
    {
        return std::visit(
            [this](const auto& concreteAction)
            {
                return ApplyConcreteAction(concreteAction);
            },
            action);
    }

private:
    AutomationStatus ApplyConcreteAction(const CreateWindowDocumentRequest& request)
    {
        std::filesystem::path windowFile = request.windowFile.value_or(request.window.sourceFile);
        windowFile = windowFile.lexically_normal();
        if (windowFile.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Window file cannot be empty.");
        }
        if (IsExecStagingPath(windowFile))
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument,
                                 "Choose a source assets folder for the window JSON, not a staged _Exec folder.");
        }

        mfd::LoadedWindowConfiguration next {};
        next.window = request.window;
        next.window.sourceFile = windowFile;
        next.document.sourceFile = windowFile;
        next.generatedTransportMap.reset();

        const std::filesystem::path reticleLibraryFolder =
            ResolveAgainst(windowFile,
                           request.reticleLibraryFolder.value_or(
                               request.window.reticleLibraryFolder.empty() ? DefaultReticleLibraryFolder(windowFile)
                                                                            : request.window.reticleLibraryFolder));
        next.window.reticleLibraryFolder = reticleLibraryFolder;
        next.document.reticleLibraryFolder = reticleLibraryFolder;

        editor::EditorFileLayout nextFiles {};
        if (request.initialPage.has_value())
        {
            mfd::PageDefinition page = *request.initialPage;
            if (page.name.empty())
            {
                return FailureStatus(AutomationErrorCode::InvalidArgument, "Initial page name cannot be empty.");
            }

            page.normalizedName = mfd::NormalizePageName(page.name);
            BootstrapEditorLayersForPage(page);
            next.document.pages.push_back(page);

            std::filesystem::path pageFile =
                ResolveAgainst(windowFile,
                               request.initialPageFile.value_or(editor::DefaultPageFilePath(windowFile, page.name)));
            if (IsExecStagingPath(pageFile))
            {
                return FailureStatus(AutomationErrorCode::InvalidArgument,
                                     "Choose a source assets folder for the first page JSON, not a staged _Exec folder.");
            }

            nextFiles.pageFiles.push_back(pageFile);
            next.window.pageFiles = nextFiles.pageFiles;
        }

        context_.bridge.MutableLoaded() = std::move(next);
        context_.bridge.MutableFiles() = std::move(nextFiles);
        context_.bridge.SetWindowFile(windowFile);
        if (!context_.bridge.MutableLoaded().document.pages.empty())
        {
            context_.bridge.SelectPage(0);
        }

        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakeWindowId(context_.bridge.Loaded()).value,
                             "Automation created a new authored window document.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const CreatePageAssetRequest& request)
    {
        if (!context_.bridge.HasOpenWindow())
        {
            return FailureStatus(AutomationErrorCode::InvalidState, "No authored window is currently open.");
        }
        if (request.page.name.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Page name cannot be empty.");
        }
        if (HasDuplicatePageName(context_.bridge.Loaded(), request.page.name))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Page ids must stay unique inside one window.");
        }

        mfd::PageDefinition page = request.page;
        page.normalizedName = mfd::NormalizePageName(page.name);
        BootstrapEditorLayersForPage(page);

        std::filesystem::path pageFile =
            ResolveAgainst(context_.bridge.WindowFile(),
                           request.filePathHint.value_or(editor::DefaultPageFilePath(context_.bridge.WindowFile(), page.name)));
        if (pageFile.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Page file cannot be empty.");
        }
        if (IsExecStagingPath(pageFile))
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument,
                                 "Choose a source assets folder for the page JSON, not a staged _Exec folder.");
        }

        context_.bridge.MutableLoaded().document.pages.push_back(page);
        context_.bridge.MutableFiles().pageFiles.push_back(pageFile);
        SyncWindowPageFiles(context_.bridge.MutableLoaded(), context_.bridge.Files());
        context_.bridge.SelectPage(static_cast<int>(context_.bridge.Loaded().document.pages.size()) - 1);
        context_.events.Push(AutomationEventKind::DocumentChanged, MakePageId(page).value, "Automation created a new page asset.");
        context_.events.Push(AutomationEventKind::ActivePageChanged, MakePageId(page).value, "Automation selected the new page.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const ReplacePageAssetRequest& request)
    {
        int pageIndex = -1;
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, &pageIndex);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }
        if (request.page.name.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Page name cannot be empty.");
        }
        if (HasDuplicatePageName(context_.bridge.Loaded(), request.page.name, pageIndex))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Page ids must stay unique inside one window.");
        }

        mfd::PageDefinition replacement = request.page;
        replacement.normalizedName = mfd::NormalizePageName(replacement.name);
        BootstrapEditorLayersForPage(replacement);
        *page = replacement;

        if (request.filePathHint.has_value())
        {
            std::filesystem::path nextFile = ResolveAgainst(context_.bridge.WindowFile(), *request.filePathHint);
            if (IsExecStagingPath(nextFile))
            {
                return FailureStatus(AutomationErrorCode::InvalidArgument,
                                     "Choose a source assets folder for the page JSON, not a staged _Exec folder.");
            }

            context_.bridge.MutableFiles().pageFiles[static_cast<std::size_t>(pageIndex)] = nextFile;
            SyncWindowPageFiles(context_.bridge.MutableLoaded(), context_.bridge.Files());
        }

        context_.bridge.SelectPage(pageIndex);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakePageId(replacement).value,
                             "Automation replaced one page asset.");
        context_.events.Push(AutomationEventKind::ActivePageChanged,
                             MakePageId(replacement).value,
                             "Automation focused the updated page.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const DeletePageAssetRequest& request)
    {
        int pageIndex = -1;
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, &pageIndex);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }

        const std::string deletedPageId = request.pageId.value;
        if (pageIndex >= 0 && pageIndex < static_cast<int>(context_.bridge.Files().pageFiles.size()))
        {
            context_.bridge.MutableFiles().removedPageFiles.push_back(
                context_.bridge.Files().pageFiles[static_cast<std::size_t>(pageIndex)]);
            context_.bridge.MutableFiles().pageFiles.erase(
                context_.bridge.MutableFiles().pageFiles.begin() + pageIndex);
        }

        context_.bridge.MutableLoaded().document.pages.erase(
            context_.bridge.MutableLoaded().document.pages.begin() + pageIndex);
        SyncWindowPageFiles(context_.bridge.MutableLoaded(), context_.bridge.Files());

        if (context_.bridge.HasOpenWindow() && !context_.bridge.Loaded().document.pages.empty())
        {
            context_.bridge.SelectPage(std::clamp(pageIndex, 0, static_cast<int>(context_.bridge.Loaded().document.pages.size()) - 1));
        }

        context_.events.Push(AutomationEventKind::DocumentChanged, deletedPageId, "Automation deleted one page asset.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const CreateReticleAssetRequest& request)
    {
        if (!context_.bridge.HasOpenWindow())
        {
            return FailureStatus(AutomationErrorCode::InvalidState, "No authored window is currently open.");
        }
        if (request.reticle.id.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Reticle asset id cannot be empty.");
        }
        if (HasDuplicateReticleAssetId(context_.bridge.Loaded(), request.reticle.id))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Reticle asset ids must stay unique inside the library.");
        }

        mfd::ReticleGroup reticle = request.reticle;
        reticle.sourceTemplateId.clear();
        const std::filesystem::path templateFile =
            ResolveAgainst(context_.bridge.Loaded().window.reticleLibraryFolder,
                           request.filePathHint.value_or(
                               editor::DefaultTemplateFilePath(context_.bridge.Loaded().window.reticleLibraryFolder, reticle.id)));
        if (IsExecStagingPath(templateFile))
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument,
                                 "Choose a source assets folder for the reticle template JSON, not a staged _Exec folder.");
        }

        context_.bridge.MutableLoaded().document.reticleLibrary[reticle.id] = reticle;
        context_.bridge.MutableFiles().templateFiles[reticle.id] = templateFile;
        context_.bridge.SelectReticleAsset(reticle.id);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakeReticleAssetId(reticle.id).value,
                             "Automation created a new reticle asset.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const ReplaceReticleAssetRequest& request)
    {
        mfd::ReticleGroup* existing = FindReticleAssetById(context_.bridge.MutableLoaded(), request.reticleId);
        if (existing == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
        }

        std::string oldKey = existing->id;
        mfd::ReticleGroup replacement = request.reticle;
        if (replacement.id.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Reticle asset id cannot be empty.");
        }
        if (HasDuplicateReticleAssetId(context_.bridge.Loaded(), replacement.id, oldKey))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Reticle asset ids must stay unique inside the library.");
        }

        replacement.sourceTemplateId.clear();
        if (mfd::NormalizePageName(oldKey) != mfd::NormalizePageName(replacement.id))
        {
            context_.bridge.MutableLoaded().document.reticleLibrary.erase(oldKey);
            context_.bridge.MutableLoaded().document.reticleLibrary[replacement.id] = replacement;

            auto templateFile = context_.bridge.MutableFiles().templateFiles.extract(oldKey);
            if (templateFile.empty())
            {
                context_.bridge.MutableFiles().templateFiles[replacement.id] =
                    editor::DefaultTemplateFilePath(context_.bridge.Loaded().window.reticleLibraryFolder, replacement.id);
            }
            else
            {
                templateFile.key() = replacement.id;
                context_.bridge.MutableFiles().templateFiles.insert(std::move(templateFile));
            }

            for (mfd::PageDefinition& page : context_.bridge.MutableLoaded().document.pages)
            {
                for (mfd::ReticleGroup& pageReticle : page.staticReticles)
                {
                    if (mfd::NormalizePageName(pageReticle.sourceTemplateId) == mfd::NormalizePageName(oldKey))
                    {
                        pageReticle.sourceTemplateId = replacement.id;
                    }
                }
            }
        }
        else
        {
            *existing = replacement;
        }

        if (request.filePathHint.has_value())
        {
            context_.bridge.MutableFiles().templateFiles[replacement.id] =
                ResolveAgainst(context_.bridge.Loaded().window.reticleLibraryFolder, *request.filePathHint);
        }

        context_.bridge.SelectReticleAsset(replacement.id);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakeReticleAssetId(replacement.id).value,
                             "Automation replaced one reticle asset.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const DeleteReticleAssetRequest& request)
    {
        mfd::ReticleGroup* existing = FindReticleAssetById(context_.bridge.MutableLoaded(), request.reticleId);
        if (existing == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
        }

        for (const mfd::PageDefinition& page : context_.bridge.Loaded().document.pages)
        {
            for (const mfd::ReticleGroup& reticle : page.staticReticles)
            {
                if (mfd::NormalizePageName(reticle.sourceTemplateId) == mfd::NormalizePageName(existing->id))
                {
                    return FailureStatus(AutomationErrorCode::Conflict,
                                         "Cannot delete one reticle asset while page reticles still reference it.");
                }
            }
        }

        const std::string deletedId = existing->id;
        context_.bridge.MutableLoaded().document.reticleLibrary.erase(deletedId);
        if (const auto iterator = context_.bridge.Files().templateFiles.find(deletedId);
            iterator != context_.bridge.Files().templateFiles.end())
        {
            context_.bridge.MutableFiles().removedTemplateFiles.push_back(iterator->second);
            context_.bridge.MutableFiles().templateFiles.erase(deletedId);
        }

        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.reticleId.value,
                             "Automation deleted one reticle asset.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const InstantiateReticleOnPageRequest& request)
    {
        int pageIndex = -1;
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, &pageIndex);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }

        const mfd::ReticleGroup* templateReticle = FindReticleAssetById(context_.bridge.Loaded(), request.reticleAssetId);
        if (templateReticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
        }

        const std::string instanceId = request.instanceId.has_value()
                                           ? *request.instanceId
                                           : MakeUniqueReticleId(page->staticReticles, templateReticle->id);
        if (instanceId.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Page reticle id cannot be empty.");
        }
        if (HasDuplicatePageReticleId(*page, instanceId))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Page reticle ids must stay unique inside one page.");
        }

        mfd::ReticleGroup instance = mfd::InstantiateReticle(*templateReticle, instanceId, request.transform, {});
        if (request.assignDefaultLayer && instance.editor.layerId.empty())
        {
            BootstrapEditorLayersForPage(*page);
            instance.editor.layerId = DefaultEditorLayerId(*page);
        }

        page->staticReticles.push_back(std::move(instance));
        context_.bridge.SelectPageReticle(pageIndex, static_cast<int>(page->staticReticles.size()) - 1);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakePageReticleInstanceId(*page, page->staticReticles.back()).value,
                             "Automation instantiated one library reticle on a page.");
        context_.events.Push(AutomationEventKind::SelectionChanged,
                             MakePageReticleInstanceId(*page, page->staticReticles.back()).value,
                             "Automation selected the new page reticle.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const ReplacePageReticleInstanceRequest& request)
    {
        mfd::PageDefinition* page = nullptr;
        int pageIndex = -1;
        int reticleIndex = -1;
        mfd::ReticleGroup* reticle =
            FindPageReticleById(context_.bridge.MutableLoaded(), request.pageReticleId, &page, &pageIndex, &reticleIndex);
        if (reticle == nullptr || page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page reticle id.");
        }
        if (request.reticle.id.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Page reticle id cannot be empty.");
        }
        if (HasDuplicatePageReticleId(*page, request.reticle.id, reticleIndex))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Page reticle ids must stay unique inside one page.");
        }

        *reticle = request.reticle;
        context_.bridge.SelectPageReticle(pageIndex, reticleIndex);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakePageReticleInstanceId(*page, *reticle).value,
                             "Automation replaced one page reticle instance.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const DeletePageReticleInstanceRequest& request)
    {
        mfd::PageDefinition* page = nullptr;
        int pageIndex = -1;
        int reticleIndex = -1;
        mfd::ReticleGroup* reticle =
            FindPageReticleById(context_.bridge.MutableLoaded(), request.pageReticleId, &page, &pageIndex, &reticleIndex);
        if (reticle == nullptr || page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page reticle id.");
        }

        page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);
        if (!page->staticReticles.empty())
        {
            context_.bridge.SelectPageReticle(pageIndex, std::clamp(reticleIndex, 0, static_cast<int>(page->staticReticles.size()) - 1));
        }
        else
        {
            context_.bridge.SelectPage(pageIndex);
        }

        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageReticleId.value,
                             "Automation deleted one page reticle instance.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const AddPrimitiveRequest& request)
    {
        PrimitiveOwnerAccess owner = ResolvePrimitiveOwner(context_.bridge.MutableLoaded(), request.ownerKind, request.ownerId);
        if (owner.primitives == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown primitive owner id.");
        }
        if (HasDuplicatePrimitiveId(*owner.primitives, request.primitive.id))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Primitive ids must stay unique inside one reticle.");
        }

        const int insertIndex = request.insertIndex.has_value()
                                    ? std::clamp(*request.insertIndex, 0, static_cast<int>(owner.primitives->size()))
                                    : static_cast<int>(owner.primitives->size());
        owner.primitives->insert(owner.primitives->begin() + insertIndex, request.primitive);
        context_.events.Push(AutomationEventKind::DocumentChanged, owner.ownerId, "Automation appended one primitive.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const ReplacePrimitiveRequest& request)
    {
        PrimitiveOwnerAccess owner = ResolvePrimitiveOwner(context_.bridge.MutableLoaded(), request.ownerKind, request.ownerId);
        if (owner.primitives == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown primitive owner id.");
        }

        const int primitiveIndex = ResolvePrimitiveIndex(*owner.primitives, request.primitive);
        if (primitiveIndex < 0)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown primitive id.");
        }
        if (HasDuplicatePrimitiveId(*owner.primitives, request.replacement.id, primitiveIndex))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Primitive ids must stay unique inside one reticle.");
        }

        (*owner.primitives)[static_cast<std::size_t>(primitiveIndex)] = request.replacement;
        context_.events.Push(AutomationEventKind::DocumentChanged, owner.ownerId, "Automation replaced one primitive.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const DeletePrimitiveRequest& request)
    {
        PrimitiveOwnerAccess owner = ResolvePrimitiveOwner(context_.bridge.MutableLoaded(), request.ownerKind, request.ownerId);
        if (owner.primitives == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown primitive owner id.");
        }

        const int primitiveIndex = ResolvePrimitiveIndex(*owner.primitives, request.primitive);
        if (primitiveIndex < 0)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown primitive id.");
        }

        owner.primitives->erase(owner.primitives->begin() + primitiveIndex);
        context_.events.Push(AutomationEventKind::DocumentChanged, owner.ownerId, "Automation deleted one primitive.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetReticleVisibilityRequest& request)
    {
        mfd::ReticleGroup* reticle = FindPageReticleById(context_.bridge.MutableLoaded(), request.pageReticleId);
        if (reticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page reticle id.");
        }

        reticle->visible = request.visible;
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageReticleId.value,
                             "Automation updated page reticle visibility.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetReticleDrawOnTopRequest& request)
    {
        mfd::ReticleGroup* reticle = FindPageReticleById(context_.bridge.MutableLoaded(), request.pageReticleId);
        if (reticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page reticle id.");
        }

        reticle->drawOnTop = request.drawOnTop;
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageReticleId.value,
                             "Automation updated page reticle draw-on-top state.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetPrimitiveVisibilityRequest& request)
    {
        PrimitiveOwnerAccess owner = ResolvePrimitiveOwner(context_.bridge.MutableLoaded(), request.ownerKind, request.ownerId);
        if (owner.primitives == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown primitive owner id.");
        }

        const int primitiveIndex = ResolvePrimitiveIndex(*owner.primitives, request.primitive);
        if (primitiveIndex < 0)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown primitive id.");
        }

        (*owner.primitives)[static_cast<std::size_t>(primitiveIndex)].style.visible = request.visible;
        context_.events.Push(AutomationEventKind::DocumentChanged, owner.ownerId, "Automation updated primitive visibility.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetLayerVisibilityRequest& request)
    {
        mfd::EditorLayerDefinition* layer =
            FindLayerById(context_.bridge.MutableLoaded(), request.pageId, request.layerId, nullptr);
        if (layer == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown layer id.");
        }

        layer->visible = request.visible;
        context_.events.Push(AutomationEventKind::DocumentChanged, request.layerId.value, "Automation updated layer visibility.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SelectEntityRequest& request)
    {
        switch (request.selectionKind)
        {
        case AutomationSelectionKind::Page:
        {
            int pageIndex = -1;
            if (FindPageById(context_.bridge.MutableLoaded(), PageId {request.targetId}, &pageIndex) == nullptr)
            {
                return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
            }

            context_.bridge.SelectPage(pageIndex);
            context_.events.Push(AutomationEventKind::SelectionChanged, request.targetId, "Automation selected one page.");
            return AutomationStatus::Success();
        }
        case AutomationSelectionKind::PageReticleInstance:
        {
            int pageIndex = -1;
            int reticleIndex = -1;
            if (FindPageReticleById(context_.bridge.MutableLoaded(),
                                    PageReticleInstanceId {request.targetId},
                                    nullptr,
                                    &pageIndex,
                                    &reticleIndex) == nullptr)
            {
                return FailureStatus(AutomationErrorCode::NotFound, "Unknown page reticle id.");
            }

            context_.bridge.SelectPageReticle(pageIndex, reticleIndex);
            context_.events.Push(AutomationEventKind::SelectionChanged,
                                 request.targetId,
                                 "Automation selected one page reticle.");
            return AutomationStatus::Success();
        }
        case AutomationSelectionKind::ReticleAsset:
            if (FindReticleAssetById(context_.bridge.MutableLoaded(), ReticleAssetId {request.targetId}) == nullptr)
            {
                return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
            }

            context_.bridge.SelectReticleAsset(request.targetId);
            context_.events.Push(AutomationEventKind::SelectionChanged, request.targetId, "Automation selected one reticle asset.");
            return AutomationStatus::Success();
        case AutomationSelectionKind::Primitive:
        case AutomationSelectionKind::None:
            break;
        }

        return FailureStatus(AutomationErrorCode::Unsupported, "Unsupported selection target.");
    }

    AutomationCoreContext& context_;
};

class AutomationPersistenceService final : public IEditorAutomationPersistenceService
{
public:
    explicit AutomationPersistenceService(AutomationCoreContext& context)
        : context_(context)
    {
    }

    [[nodiscard]] AutomationResult<ValidationReport> ValidateCurrentState() const override
    {
        return AutomationResult<ValidationReport>::Success(
            ValidateAutomationState(context_.bridge.Loaded(), context_.bridge.Files()));
    }

    [[nodiscard]] AutomationResult<SaveResult> SaveAll() override
    {
        if (context_.activeSession.has_value())
        {
            return FailureResult<SaveResult>(AutomationErrorCode::InvalidState,
                                             "Commit or roll back the active preview session before saving.");
        }

        std::string error;
        if (!editor::SaveEditorDocument(context_.bridge.Loaded(), context_.bridge.Files(), &error))
        {
            return FailureResult<SaveResult>(AutomationErrorCode::IoFailure, "Save failed: " + error);
        }

        SaveResult result;
        result.savedFiles = CollectSavedFiles(context_.bridge.Loaded(), context_.bridge.Files());
        context_.events.Push(AutomationEventKind::AssetSaved,
                             MakeWindowId(context_.bridge.Loaded()).value,
                             "Automation saved authored assets through the editor serializer.");
        return AutomationResult<SaveResult>::Success(std::move(result));
    }

    [[nodiscard]] AutomationResult<SaveResult> SaveAsset(const SaveAssetRequest& request) override
    {
        static_cast<void>(request);
        return SaveAll();
    }

    [[nodiscard]] AutomationResult<ExportJsonPreviewResult> ExportJsonPreview(const ExportJsonPreviewRequest& request) const override
    {
        ExportJsonPreviewResult result;

        switch (request.kind)
        {
        case ExportJsonPreviewRequest::Kind::Window:
            result.sourcePath = context_.bridge.Loaded().window.sourceFile;
            result.json = editor::SerializeWindowToJsonString(context_.bridge.Loaded().window,
                                                              context_.bridge.Loaded().document,
                                                              context_.bridge.Files());
            return AutomationResult<ExportJsonPreviewResult>::Success(std::move(result));
        case ExportJsonPreviewRequest::Kind::Page:
        {
            int pageIndex = -1;
            const mfd::PageDefinition* page = FindPageById(context_.bridge.Loaded(), PageId {request.entityId}, &pageIndex);
            if (page == nullptr)
            {
                return FailureResult<ExportJsonPreviewResult>(AutomationErrorCode::NotFound, "Unknown page id.");
            }

            result.sourcePath = pageIndex >= 0 && pageIndex < static_cast<int>(context_.bridge.Files().pageFiles.size())
                                    ? context_.bridge.Files().pageFiles[static_cast<std::size_t>(pageIndex)]
                                    : std::filesystem::path {};
            result.json = editor::SerializePageToJsonString(*page,
                                                            context_.bridge.Loaded().document.reticleLibrary,
                                                            context_.bridge.Files(),
                                                            static_cast<std::size_t>(pageIndex));
            return AutomationResult<ExportJsonPreviewResult>::Success(std::move(result));
        }
        case ExportJsonPreviewRequest::Kind::ReticleAsset:
        {
            const mfd::ReticleGroup* reticle =
                FindReticleAssetById(context_.bridge.Loaded(), ReticleAssetId {request.entityId});
            if (reticle == nullptr)
            {
                return FailureResult<ExportJsonPreviewResult>(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
            }

            const auto iterator = context_.bridge.Files().templateFiles.find(reticle->id);
            result.sourcePath = iterator == context_.bridge.Files().templateFiles.end() ? std::filesystem::path {}
                                                                                        : iterator->second;
            result.json = editor::SerializeReticleTemplateToJsonString(*reticle, result.sourcePath.parent_path());
            return AutomationResult<ExportJsonPreviewResult>::Success(std::move(result));
        }
        case ExportJsonPreviewRequest::Kind::PageReticleInstance:
        {
            const mfd::ReticleGroup* reticle =
                FindPageReticleById(context_.bridge.Loaded(), PageReticleInstanceId {request.entityId});
            if (reticle == nullptr)
            {
                return FailureResult<ExportJsonPreviewResult>(AutomationErrorCode::NotFound, "Unknown page reticle id.");
            }

            result.json = editor::SerializePageReticleToJsonString(
                *reticle,
                context_.bridge.Loaded().document.reticleLibrary,
                context_.bridge.Loaded().window.sourceFile.parent_path());
            return AutomationResult<ExportJsonPreviewResult>::Success(std::move(result));
        }
        }

        return FailureResult<ExportJsonPreviewResult>(AutomationErrorCode::Unsupported, "Unsupported JSON preview export request.");
    }

private:
    AutomationCoreContext& context_;
};

class AutomationSessionService final : public IEditorAutomationSessionService
{
public:
    AutomationSessionService(AutomationCoreContext& context,
                             IEditorAutomationAuthoringService& authoringService,
                             IEditorAutomationPersistenceService& persistenceService)
        : context_(context)
        , authoringService_(authoringService)
        , persistenceService_(persistenceService)
    {
    }

    [[nodiscard]] AutomationResult<AutomationSessionId> BeginSession(const BeginSessionRequest& request) override
    {
        if (context_.activeSession.has_value())
        {
            return FailureResult<AutomationSessionId>(AutomationErrorCode::InvalidState,
                                                      "Only one preview session may stay active at a time.");
        }

        SessionState session;
        session.id.value = "session:" + std::to_string(context_.nextSessionSerial++);
        session.label = request.label;
        session.preSessionSnapshot = context_.bridge.CaptureSnapshot();
        context_.activeSession = session;
        return AutomationResult<AutomationSessionId>::Success(session.id);
    }

    AutomationStatus ApplyAction(const ApplyActionRequest& request) override
    {
        if (!context_.activeSession.has_value() || context_.activeSession->id.value != request.sessionId.value)
        {
            return FailureStatus(AutomationErrorCode::InvalidState, "Unknown or inactive preview session.");
        }

        return authoringService_.ApplyAction(request.action);
    }

    [[nodiscard]] AutomationResult<ValidationReport> ValidateSession(const ValidateSessionRequest& request) const override
    {
        if (!context_.activeSession.has_value() || context_.activeSession->id.value != request.sessionId.value)
        {
            return FailureResult<ValidationReport>(AutomationErrorCode::InvalidState, "Unknown or inactive preview session.");
        }

        return persistenceService_.ValidateCurrentState();
    }

    AutomationStatus CommitSession(const CommitSessionRequest& request) override
    {
        if (!context_.activeSession.has_value() || context_.activeSession->id.value != request.sessionId.value)
        {
            return FailureStatus(AutomationErrorCode::InvalidState, "Unknown or inactive preview session.");
        }

        context_.bridge.PushUndoSnapshot(context_.activeSession->preSessionSnapshot);
        const std::string sessionId = context_.activeSession->id.value;
        context_.activeSession.reset();
        context_.events.Push(AutomationEventKind::SessionCommitted, sessionId, "Automation committed the active preview session.");
        return AutomationStatus::Success();
    }

    AutomationStatus RollbackSession(const RollbackSessionRequest& request) override
    {
        if (!context_.activeSession.has_value() || context_.activeSession->id.value != request.sessionId.value)
        {
            return FailureStatus(AutomationErrorCode::InvalidState, "Unknown or inactive preview session.");
        }

        const EditorStateSnapshot snapshot = context_.activeSession->preSessionSnapshot;
        const std::string sessionId = context_.activeSession->id.value;
        context_.bridge.RestoreSnapshot(snapshot);
        context_.activeSession.reset();
        context_.events.Push(AutomationEventKind::SessionRolledBack, sessionId, "Automation rolled back the active preview session.");
        return AutomationStatus::Success();
    }

    [[nodiscard]] std::optional<AutomationSessionId> ActiveSessionId() const override
    {
        return context_.activeSession.has_value() ? std::optional<AutomationSessionId> {context_.activeSession->id}
                                                  : std::nullopt;
    }

private:
    AutomationCoreContext& context_;
    IEditorAutomationAuthoringService& authoringService_;
    IEditorAutomationPersistenceService& persistenceService_;
};

class AutomationEventService final : public IEditorAutomationEventService
{
public:
    explicit AutomationEventService(AutomationCoreContext& context)
        : context_(context)
    {
    }

    [[nodiscard]] std::vector<AutomationEvent> ConsumePendingEvents() override
    {
        return context_.events.Consume();
    }

private:
    AutomationCoreContext& context_;
};

class Facade final : public IEditorAutomationFacade
{
public:
    explicit Facade(IEditorAutomationEditorBridge& bridge)
        : context_ {bridge}
        , queryService_ {context_}
        , authoringService_ {context_}
        , persistenceService_ {context_}
        , sessionService_ {context_, authoringService_, persistenceService_}
        , eventService_ {context_}
    {
    }

    [[nodiscard]] IEditorAutomationQueryService& QueryService() noexcept override
    {
        return queryService_;
    }

    [[nodiscard]] IEditorAutomationAuthoringService& AuthoringService() noexcept override
    {
        return authoringService_;
    }

    [[nodiscard]] IEditorAutomationPersistenceService& PersistenceService() noexcept override
    {
        return persistenceService_;
    }

    [[nodiscard]] IEditorAutomationSessionService& SessionService() noexcept override
    {
        return sessionService_;
    }

    [[nodiscard]] IEditorAutomationEventService& EventService() noexcept override
    {
        return eventService_;
    }

private:
    AutomationCoreContext context_;
    AutomationQueryService queryService_;
    AutomationAuthoringService authoringService_;
    AutomationPersistenceService persistenceService_;
    AutomationSessionService sessionService_;
    AutomationEventService eventService_;
};
} // namespace

std::unique_ptr<IEditorAutomationFacade> CreateEditorAutomationFacade(IEditorAutomationEditorBridge& bridge)
{
    return std::make_unique<Facade>(bridge);
}
} // namespace editor::automation
