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
#include <fstream>
#include <optional>
#include <stdexcept>
#include <utility>

#include "EditorAutomationBridge.h"
#include "EditorAutomationDocumentUtils.h"
#include "PageManagementService.h"

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

std::filesystem::path ResolveAgainstDirectory(const std::filesystem::path& anchorDirectory,
                                              const std::filesystem::path& candidate)
{
    if (candidate.empty())
    {
        return {};
    }

    if (candidate.is_absolute())
    {
        return candidate.lexically_normal();
    }

    if (anchorDirectory.empty())
    {
        return candidate.lexically_normal();
    }

    return (anchorDirectory / candidate).lexically_normal();
}

std::filesystem::path DefaultReticleLibraryFolder(const std::filesystem::path& windowFile)
{
    if (windowFile.empty())
    {
        return {};
    }

    return (windowFile.parent_path() / "reticles").lexically_normal();
}

std::optional<std::string> ExtractReticleAssetTemplateId(const std::string_view automationId)
{
    constexpr std::string_view kReticleAssetPrefix = "reticle-asset:";
    if (automationId.size() <= kReticleAssetPrefix.size() ||
        automationId.compare(0, kReticleAssetPrefix.size(), kReticleAssetPrefix) != 0)
    {
        return std::nullopt;
    }

    return std::string(automationId.substr(kReticleAssetPrefix.size()));
}

struct PrimitiveOwnerAccess
{
    std::vector<mfd::Primitive>* primitives = nullptr;
    std::string ownerId {};
};

struct ReticleTargetAccess
{
    mfd::ReticleGroup* reticle = nullptr;
    std::string targetId {};
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

bool SupportsPrimitiveLineStyle(const mfd::PrimitiveType type) noexcept
{
    switch (type)
    {
    case mfd::PrimitiveType::Text:
    case mfd::PrimitiveType::Time:
    case mfd::PrimitiveType::Image:
        return false;
    default:
        return true;
    }
}

mfd::PageStrobeDefinition* FindPageStrobeByPageId(mfd::LoadedWindowConfiguration& loaded,
                                                  const PageId& pageId,
                                                  mfd::PageDefinition** page = nullptr,
                                                  int* pageIndex = nullptr)
{
    mfd::PageDefinition* targetPage = FindPageById(loaded, pageId, pageIndex);
    if (targetPage == nullptr || !targetPage->strobe.has_value())
    {
        return nullptr;
    }

    if (page != nullptr)
    {
        *page = targetPage;
    }

    return &*targetPage->strobe;
}

const mfd::PageStrobeDefinition* FindPageStrobeByPageId(const mfd::LoadedWindowConfiguration& loaded,
                                                        const PageId& pageId,
                                                        const mfd::PageDefinition** page = nullptr,
                                                        int* pageIndex = nullptr)
{
    const mfd::PageDefinition* targetPage = FindPageById(loaded, pageId, pageIndex);
    if (targetPage == nullptr || !targetPage->strobe.has_value())
    {
        return nullptr;
    }

    if (page != nullptr)
    {
        *page = targetPage;
    }

    return &*targetPage->strobe;
}

ReticleTargetAccess ResolveReticleTarget(mfd::LoadedWindowConfiguration& loaded,
                                         const AutomationReticleTargetKind targetKind,
                                         const std::string_view targetId)
{
    ReticleTargetAccess result;
    result.targetId = std::string(targetId);

    switch (targetKind)
    {
    case AutomationReticleTargetKind::ReticleAsset:
        result.reticle = FindReticleAssetById(loaded, ReticleAssetId {std::string(targetId)});
        return result;
    case AutomationReticleTargetKind::PageReticleInstance:
        result.reticle = FindPageReticleById(loaded, PageReticleInstanceId {std::string(targetId)});
        return result;
    case AutomationReticleTargetKind::PageStrobe:
    {
        mfd::PageStrobeDefinition* strobe = FindPageStrobeByPageId(loaded, PageId {std::string(targetId)}, nullptr, nullptr);
        result.reticle = strobe == nullptr ? nullptr : &strobe->reticle;
        return result;
    }
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

bool HasDuplicateLayerId(const mfd::PageDefinition& page, const std::string_view layerId, const int ignoredIndex = -1)
{
    const std::string normalizedLayerId = mfd::NormalizePageName(layerId);
    for (int index = 0; index < static_cast<int>(page.editor.layers.size()); ++index)
    {
        if (index == ignoredIndex)
        {
            continue;
        }

        if (mfd::NormalizePageName(page.editor.layers[static_cast<std::size_t>(index)].id) == normalizedLayerId)
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

std::optional<int> ResolveReplacementPageIndex(const mfd::LoadedWindowConfiguration& loaded,
                                               const std::optional<PageId>& replacementPageId)
{
    if (!replacementPageId.has_value())
    {
        return std::nullopt;
    }

    int replacementPageIndex = -1;
    const mfd::PageDefinition* replacementPage = FindPageById(loaded, *replacementPageId, &replacementPageIndex);
    if (replacementPage == nullptr || replacementPageIndex < 0)
    {
        return std::nullopt;
    }

    return replacementPageIndex;
}

std::vector<std::filesystem::path> CollectSavedFiles(const mfd::LoadedWindowConfiguration& loaded, const editor::EditorFileLayout& files)
{
    std::vector<std::filesystem::path> savedFiles;
    if (!loaded.window.sourceFile.empty())
    {
        savedFiles.push_back(loaded.window.sourceFile);
    }

    savedFiles.insert(savedFiles.end(), files.pageFiles.begin(), files.pageFiles.end());
    for (const auto& entry : files.templateFiles)
    {
        savedFiles.push_back(entry.second);
    }

    std::sort(savedFiles.begin(), savedFiles.end());
    savedFiles.erase(std::unique(savedFiles.begin(), savedFiles.end()), savedFiles.end());
    return savedFiles;
}

std::size_t FindBlinkTypeIndex(const mfd::PageDefinition& page, const std::string_view blinkTypeName)
{
    const std::string normalizedBlinkTypeName = mfd::NormalizePageName(blinkTypeName);
    if (normalizedBlinkTypeName.empty())
    {
        return static_cast<std::size_t>(-1);
    }

    for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
    {
        if (page.blinkTypes[index].normalizedName == normalizedBlinkTypeName)
        {
            return index;
        }
    }

    return static_cast<std::size_t>(-1);
}

std::size_t EffectiveDefaultBlinkTypeIndex(const mfd::PageDefinition& page)
{
    if (page.blinkTypes.empty())
    {
        return static_cast<std::size_t>(-1);
    }

    if (!page.normalizedDefaultBlinkTypeName.empty())
    {
        for (std::size_t index = 0; index < page.blinkTypes.size(); ++index)
        {
            if (page.blinkTypes[index].normalizedName == page.normalizedDefaultBlinkTypeName)
            {
                return index;
            }
        }
    }

    return 0U;
}

void RefreshBlinkBinding(const mfd::PageDefinition& page, mfd::ReticleBlinkState& blink)
{
    blink.normalizedTypeName = blink.typeName.empty() ? std::string {} : mfd::NormalizePageName(blink.typeName);

    if (!blink.typeName.empty())
    {
        const std::size_t blinkTypeIndex = FindBlinkTypeIndex(page, blink.normalizedTypeName);
        blink.durationMs = blinkTypeIndex == static_cast<std::size_t>(-1) ? 0U : page.blinkTypes[blinkTypeIndex].durationMs;
        return;
    }

    if (!blink.enabled)
    {
        blink.durationMs = 0U;
        return;
    }

    const std::size_t defaultBlinkTypeIndex = EffectiveDefaultBlinkTypeIndex(page);
    blink.durationMs =
        defaultBlinkTypeIndex == static_cast<std::size_t>(-1) ? 0U : page.blinkTypes[defaultBlinkTypeIndex].durationMs;
}

void ClearInvalidBlinkReferences(mfd::PageDefinition& page)
{
    const auto clearInvalidReference = [&page](mfd::ReticleBlinkState& blink)
    {
        if (blink.typeName.empty())
        {
            return;
        }

        if (FindBlinkTypeIndex(page, blink.typeName) == static_cast<std::size_t>(-1))
        {
            blink = {};
        }
    };

    for (mfd::ReticleGroup& reticle : page.staticReticles)
    {
        clearInvalidReference(reticle.blink);
    }

    if (page.strobe.has_value())
    {
        clearInvalidReference(page.strobe->reticle.blink);
    }
}

void RefreshPageBlinkState(mfd::PageDefinition& page)
{
    for (mfd::PageBlinkDefinition& blinkType : page.blinkTypes)
    {
        blinkType.normalizedName = mfd::NormalizePageName(blinkType.name);
        blinkType.durationMs = std::max<std::uint32_t>(1U, blinkType.durationMs);
    }

    page.normalizedDefaultBlinkTypeName =
        page.defaultBlinkTypeName.empty() ? std::string {} : mfd::NormalizePageName(page.defaultBlinkTypeName);
    ClearInvalidBlinkReferences(page);

    for (mfd::ReticleGroup& reticle : page.staticReticles)
    {
        RefreshBlinkBinding(page, reticle.blink);
    }

    if (page.strobe.has_value())
    {
        RefreshBlinkBinding(page, page.strobe->reticle.blink);
    }
}

void WriteUtf8TextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        throw std::runtime_error("Unable to open file for writing: " + path.string());
    }

    stream << text;
}

void RenameEditorLayerReferences(mfd::PageDefinition& page,
                                 const std::string_view previousLayerId,
                                 const std::string& nextLayerId)
{
    if (previousLayerId.empty() || previousLayerId == nextLayerId)
    {
        return;
    }

    for (mfd::ReticleGroup& reticle : page.staticReticles)
    {
        if (reticle.editor.layerId == previousLayerId)
        {
            reticle.editor.layerId = nextLayerId;
        }
    }
}

std::size_t ClearEditorLayerReferences(mfd::PageDefinition& page, const std::string_view removedLayerId)
{
    if (removedLayerId.empty())
    {
        return 0U;
    }

    std::size_t clearedCount = 0U;
    for (mfd::ReticleGroup& reticle : page.staticReticles)
    {
        if (reticle.editor.layerId == removedLayerId)
        {
            reticle.editor.layerId.clear();
            ++clearedCount;
        }
    }

    return clearedCount;
}

mfd::PageStrobeDefinition MakePageStrobeFromTemplate(const mfd::PageDefinition& page,
                                                     const mfd::ReticleGroup& templ,
                                                     const std::optional<mfd::PageStrobeDefinition>& previousStrobe)
{
    const std::string baseId =
        previousStrobe.has_value() && !previousStrobe->reticle.id.empty()
            ? previousStrobe->reticle.id
            : (templ.id.empty() ? std::string {"strobe"} : templ.id + "_strobe");

    mfd::PageStrobeDefinition strobe;
    strobe.reticle = mfd::InstantiateReticle(templ, MakeUniqueReticleId(page.staticReticles, baseId));
    strobe.reticle.visible = true;

    if (previousStrobe.has_value())
    {
        strobe.reticle.visible = previousStrobe->reticle.visible;
        strobe.reticle.drawOnTop = previousStrobe->reticle.drawOnTop;
        strobe.reticle.transform = previousStrobe->reticle.transform;
        strobe.reticle.overrides = previousStrobe->reticle.overrides;
        strobe.reticle.blink = previousStrobe->reticle.blink;
        strobe.reticle.clipping = previousStrobe->reticle.clipping;
        strobe.reticle.editor = previousStrobe->reticle.editor;
        strobe.capture = previousStrobe->capture;
        strobe.magnet = previousStrobe->magnet;
    }

    return strobe;
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
            RefreshPageBlinkState(page);
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

    AutomationStatus ApplyConcreteAction(const ReplaceWindowAssetRequest& request)
    {
        if (!context_.bridge.HasOpenWindow())
        {
            return FailureStatus(AutomationErrorCode::InvalidState, "No authored window is currently open.");
        }

        std::filesystem::path windowFile =
            request.windowFile.value_or(context_.bridge.WindowFile().empty() ? request.window.sourceFile : context_.bridge.WindowFile());
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

        mfd::WindowAssetDefinition window = request.window;
        window.sourceFile = windowFile;
        window.pageFiles = context_.bridge.Files().pageFiles;
        window.reticleLibraryFolder =
            ResolveAgainst(windowFile,
                           request.reticleLibraryFolder.value_or(
                               request.window.reticleLibraryFolder.empty() ? context_.bridge.Loaded().window.reticleLibraryFolder
                                                                           : request.window.reticleLibraryFolder));

        context_.bridge.MutableLoaded().window = window;
        context_.bridge.MutableLoaded().document.sourceFile = windowFile;
        context_.bridge.MutableLoaded().document.reticleLibraryFolder = window.reticleLibraryFolder;
        context_.bridge.SetWindowFile(windowFile);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakeWindowId(context_.bridge.Loaded()).value,
                             "Automation updated the authored window settings.");
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
        RefreshPageBlinkState(page);

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
        RefreshPageBlinkState(replacement);
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

    AutomationStatus ApplyConcreteAction(const RemovePageFromWindowRequest& request)
    {
        int pageIndex = -1;
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, &pageIndex);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }

        const std::optional<int> replacementPageIndex =
            ResolveReplacementPageIndex(context_.bridge.Loaded(), request.replacementDefaultPageId);
        if (request.replacementDefaultPageId.has_value() && !replacementPageIndex.has_value())
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown replacement default page id.");
        }

        editor::PageManagementService pageManagementService;
        const editor::PageRemovePlan plan =
            pageManagementService.BuildRemovePlan(context_.bridge.Loaded(),
                                                 context_.bridge.Files(),
                                                 editor::PageRemoveRequest {pageIndex, replacementPageIndex});
        if (!plan.canExecute)
        {
            return FailureStatus(AutomationErrorCode::ValidationFailed, plan.error);
        }

        std::string error;
        if (!pageManagementService.Execute(plan, context_.bridge.MutableLoaded(), context_.bridge.MutableFiles(), &error))
        {
            return FailureStatus(AutomationErrorCode::InvalidState, error);
        }

        if (context_.bridge.HasOpenWindow() && !context_.bridge.Loaded().document.pages.empty())
        {
            context_.bridge.SelectPage(plan.nextSelectedPageIndex);
            context_.events.Push(AutomationEventKind::ActivePageChanged,
                                 MakePageId(context_.bridge.Loaded().document.pages[static_cast<std::size_t>(plan.nextSelectedPageIndex)])
                                     .value,
                                 "Automation focused the page selected after one page removal.");
        }

        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageId.value,
                             "Automation removed one page from the current window.");
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

        const std::optional<int> replacementPageIndex =
            ResolveReplacementPageIndex(context_.bridge.Loaded(), request.replacementDefaultPageId);
        if (request.replacementDefaultPageId.has_value() && !replacementPageIndex.has_value())
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown replacement default page id.");
        }

        editor::PageManagementService pageManagementService;
        const editor::PageDeletePlan plan =
            pageManagementService.BuildDeletePlan(context_.bridge.Loaded(),
                                                 context_.bridge.Files(),
                                                 editor::PageDeleteRequest {pageIndex, replacementPageIndex, {}, true});
        if (!plan.canExecute)
        {
            return FailureStatus(AutomationErrorCode::ValidationFailed, plan.error);
        }

        std::string error;
        if (!pageManagementService.Execute(plan, context_.bridge.MutableLoaded(), context_.bridge.MutableFiles(), &error))
        {
            return FailureStatus(AutomationErrorCode::InvalidState, error);
        }

        if (context_.bridge.HasOpenWindow() && !context_.bridge.Loaded().document.pages.empty())
        {
            context_.bridge.SelectPage(plan.nextSelectedPageIndex);
            context_.events.Push(AutomationEventKind::ActivePageChanged,
                                 MakePageId(context_.bridge.Loaded().document.pages[static_cast<std::size_t>(plan.nextSelectedPageIndex)])
                                     .value,
                                 "Automation focused the page selected after one page deletion.");
        }

        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageId.value,
                             "Automation deleted one page asset.");
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
            ResolveAgainstDirectory(context_.bridge.Loaded().window.reticleLibraryFolder,
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
                ResolveAgainstDirectory(context_.bridge.Loaded().window.reticleLibraryFolder, *request.filePathHint);
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
        RefreshPageBlinkState(*page);
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

    AutomationStatus ApplyConcreteAction(const MovePageReticleRequest& request)
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

        const int reticleCount = static_cast<int>(page->staticReticles.size());
        if (reticleCount <= 0)
        {
            return FailureStatus(AutomationErrorCode::InvalidState, "The parent page has no reticles to reorder.");
        }

        const int targetIndex = std::clamp(request.targetIndex, 0, reticleCount - 1);
        if (targetIndex == reticleIndex)
        {
            return AutomationStatus::Success();
        }

        mfd::ReticleGroup movedReticle = std::move(page->staticReticles[static_cast<std::size_t>(reticleIndex)]);
        page->staticReticles.erase(page->staticReticles.begin() + reticleIndex);
        page->staticReticles.insert(page->staticReticles.begin() + targetIndex, std::move(movedReticle));
        context_.bridge.SelectPageReticle(pageIndex, targetIndex);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageReticleId.value,
                             "Automation reordered one page reticle instance.");
        context_.events.Push(AutomationEventKind::SelectionChanged,
                             MakePageReticleInstanceId(*page, page->staticReticles[static_cast<std::size_t>(targetIndex)]).value,
                             "Automation kept the moved page reticle selected.");
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

    AutomationStatus ApplyConcreteAction(const SetReticleAssetVisibilityRequest& request)
    {
        mfd::ReticleGroup* reticle = FindReticleAssetById(context_.bridge.MutableLoaded(), request.reticleId);
        if (reticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
        }

        reticle->visible = request.visible;
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.reticleId.value,
                             "Automation updated reticle asset visibility.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetReticleAssetDrawOnTopRequest& request)
    {
        mfd::ReticleGroup* reticle = FindReticleAssetById(context_.bridge.MutableLoaded(), request.reticleId);
        if (reticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
        }

        reticle->drawOnTop = request.drawOnTop;
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.reticleId.value,
                             "Automation updated reticle asset draw-on-top state.");
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

    AutomationStatus ApplyConcreteAction(const SetReticleClippingRequest& request)
    {
        ReticleTargetAccess target = ResolveReticleTarget(context_.bridge.MutableLoaded(), request.targetKind, request.targetId);
        if (target.reticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle target id.");
        }

        const mfd::ReticleClipState previousClipping = target.reticle->clipping;
        target.reticle->clipping = request.clipping;

        if (target.reticle->clipping.mode != mfd::ReticleClipMode::None)
        {
            if (target.reticle->clipping.primitiveId.empty())
            {
                target.reticle->clipping = previousClipping;
                return FailureStatus(AutomationErrorCode::InvalidArgument,
                                     "Clipping requires one primitive id when the mode is not 'None'.");
            }

            if (mfd::ResolveClipPrimitive(*target.reticle) == nullptr)
            {
                target.reticle->clipping = previousClipping;
                return FailureStatus(AutomationErrorCode::ValidationFailed,
                                     "The requested clip primitive is missing or not supported for clipping.");
            }
        }

        context_.events.Push(AutomationEventKind::DocumentChanged, target.targetId, "Automation updated reticle clipping.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetReticleTransformRequest& request)
    {
        ReticleTargetAccess target = ResolveReticleTarget(context_.bridge.MutableLoaded(), request.targetKind, request.targetId);
        if (target.reticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle target id.");
        }

        target.reticle->transform = request.transform;
        context_.events.Push(AutomationEventKind::DocumentChanged, target.targetId, "Automation updated reticle transform.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetPrimitiveTransformRequest& request)
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

        (*owner.primitives)[static_cast<std::size_t>(primitiveIndex)].transform = request.transform;
        context_.events.Push(AutomationEventKind::DocumentChanged, owner.ownerId, "Automation updated primitive transform.");
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

    AutomationStatus ApplyConcreteAction(const SetPrimitiveExposedRequest& request)
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

        (*owner.primitives)[static_cast<std::size_t>(primitiveIndex)].exposed = request.exposed;
        context_.events.Push(AutomationEventKind::DocumentChanged, owner.ownerId, "Automation updated primitive exposure.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetPrimitiveLineStyleRequest& request)
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

        mfd::Primitive& primitive = (*owner.primitives)[static_cast<std::size_t>(primitiveIndex)];
        if (!SupportsPrimitiveLineStyle(primitive.type))
        {
            return FailureStatus(AutomationErrorCode::Unsupported,
                                 "The selected primitive type does not support line style.");
        }

        primitive.style.lineStyle = request.lineStyle;
        context_.events.Push(AutomationEventKind::DocumentChanged, owner.ownerId, "Automation updated primitive line style.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetPageReticleLayerRequest& request)
    {
        mfd::PageDefinition* page = nullptr;
        mfd::ReticleGroup* reticle =
            FindPageReticleById(context_.bridge.MutableLoaded(), request.pageReticleId, &page, nullptr, nullptr);
        if (reticle == nullptr || page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page reticle id.");
        }

        if (request.layerId.empty())
        {
            reticle->editor.layerId.clear();
            context_.events.Push(AutomationEventKind::DocumentChanged,
                                 request.pageReticleId.value,
                                 "Automation cleared the page reticle layer assignment.");
            return AutomationStatus::Success();
        }

        const mfd::EditorLayerDefinition* layer =
            FindLayerById(context_.bridge.Loaded(), MakePageId(*page), request.layerId, nullptr);
        if (layer == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown layer id.");
        }

        reticle->editor.layerId = layer->id;
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageReticleId.value,
                             "Automation updated page reticle layer assignment.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const CreateLayerRequest& request)
    {
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }
        if (request.layer.id.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Layer id cannot be empty.");
        }
        if (HasDuplicateLayerId(*page, request.layer.id))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Layer ids must stay unique inside one page.");
        }

        const int insertIndex = request.insertIndex.has_value()
                                    ? std::clamp(*request.insertIndex, 0, static_cast<int>(page->editor.layers.size()))
                                    : static_cast<int>(page->editor.layers.size());
        page->editor.layers.insert(page->editor.layers.begin() + insertIndex, request.layer);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakeLayerId(*page, page->editor.layers[static_cast<std::size_t>(insertIndex)]).value,
                             "Automation created one editor layer.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const ReplaceLayerRequest& request)
    {
        int layerIndex = -1;
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        mfd::EditorLayerDefinition* layer =
            FindLayerById(context_.bridge.MutableLoaded(), request.pageId, request.layerId, &layerIndex);
        if (page == nullptr || layer == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown layer id.");
        }
        if (request.layer.id.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Layer id cannot be empty.");
        }
        if (HasDuplicateLayerId(*page, request.layer.id, layerIndex))
        {
            return FailureStatus(AutomationErrorCode::Conflict, "Layer ids must stay unique inside one page.");
        }

        const std::string previousId = layer->id;
        *layer = request.layer;
        RenameEditorLayerReferences(*page, previousId, layer->id);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             MakeLayerId(*page, *layer).value,
                             "Automation replaced one editor layer.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const DeleteLayerRequest& request)
    {
        int layerIndex = -1;
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        mfd::EditorLayerDefinition* layer =
            FindLayerById(context_.bridge.MutableLoaded(), request.pageId, request.layerId, &layerIndex);
        if (page == nullptr || layer == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown layer id.");
        }

        ClearEditorLayerReferences(*page, layer->id);
        page->editor.layers.erase(page->editor.layers.begin() + layerIndex);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.layerId.value,
                             "Automation deleted one editor layer and cleared its assignments.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const ReplacePageBlinkTypesRequest& request)
    {
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }

        std::vector<mfd::PageBlinkDefinition> blinkTypes = request.blinkTypes;
        for (int index = 0; index < static_cast<int>(blinkTypes.size()); ++index)
        {
            if (blinkTypes[static_cast<std::size_t>(index)].name.empty())
            {
                return FailureStatus(AutomationErrorCode::InvalidArgument, "Blink type name cannot be empty.");
            }

            const std::string normalizedName = mfd::NormalizePageName(blinkTypes[static_cast<std::size_t>(index)].name);
            for (int otherIndex = index + 1; otherIndex < static_cast<int>(blinkTypes.size()); ++otherIndex)
            {
                if (mfd::NormalizePageName(blinkTypes[static_cast<std::size_t>(otherIndex)].name) == normalizedName)
                {
                    return FailureStatus(AutomationErrorCode::Conflict,
                                         "Blink type names must stay unique inside one page.");
                }
            }
        }

        page->blinkTypes = std::move(blinkTypes);
        RefreshPageBlinkState(*page);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageId.value,
                             "Automation replaced the page blink-type catalog.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetPageDefaultBlinkTypeRequest& request)
    {
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }

        if (!request.blinkTypeName.empty() && FindBlinkTypeIndex(*page, request.blinkTypeName) == static_cast<std::size_t>(-1))
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page blink type.");
        }

        page->defaultBlinkTypeName = request.blinkTypeName;
        RefreshPageBlinkState(*page);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageId.value,
                             "Automation updated the page default blink type.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const SetPageReticleBlinkRequest& request)
    {
        mfd::PageDefinition* page = nullptr;
        mfd::ReticleGroup* reticle =
            FindPageReticleById(context_.bridge.MutableLoaded(), request.pageReticleId, &page, nullptr, nullptr);
        if (page == nullptr || reticle == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page reticle id.");
        }

        if (!request.blink.typeName.empty() && FindBlinkTypeIndex(*page, request.blink.typeName) == static_cast<std::size_t>(-1))
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page blink type.");
        }

        reticle->blink = request.blink;
        if (!reticle->blink.enabled)
        {
            reticle->blink = {};
        }

        RefreshPageBlinkState(*page);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageReticleId.value,
                             "Automation updated the page reticle blink binding.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const AssignPageStrobeTemplateRequest& request)
    {
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }

        const mfd::ReticleGroup* templ = FindReticleAssetById(context_.bridge.Loaded(), request.reticleAssetId);
        if (templ == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
        }

        mfd::PageStrobeDefinition strobe = MakePageStrobeFromTemplate(*page, *templ, page->strobe);
        strobe.reticle.sourceTemplateId = templ->id;
        if (request.strobeId.has_value())
        {
            if (request.strobeId->empty())
            {
                return FailureStatus(AutomationErrorCode::InvalidArgument, "Page strobe id cannot be empty.");
            }
            if (HasDuplicatePageReticleId(*page, *request.strobeId))
            {
                return FailureStatus(AutomationErrorCode::Conflict,
                                     "Page strobe id must stay unique against page reticle instance ids.");
            }

            strobe.reticle.id = *request.strobeId;
        }

        page->strobe = std::move(strobe);
        RefreshPageBlinkState(*page);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageId.value,
                             "Automation assigned a strobe template to one page.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const ReplacePageStrobeRequest& request)
    {
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }
        if (request.strobe.reticle.id.empty())
        {
            return FailureStatus(AutomationErrorCode::InvalidArgument, "Page strobe id cannot be empty.");
        }
        if (HasDuplicatePageReticleId(*page, request.strobe.reticle.id))
        {
            return FailureStatus(AutomationErrorCode::Conflict,
                                 "Page strobe id must stay unique against page reticle instance ids.");
        }
        if (!request.strobe.reticle.sourceTemplateId.empty() &&
            FindReticleAssetById(context_.bridge.Loaded(),
                                 MakeReticleAssetId(request.strobe.reticle.sourceTemplateId)) == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound,
                                 "The requested page strobe template id does not exist in the reticle library.");
        }
        if (request.strobe.reticle.clipping.mode != mfd::ReticleClipMode::None &&
            mfd::ResolveClipPrimitive(request.strobe.reticle) == nullptr)
        {
            return FailureStatus(AutomationErrorCode::ValidationFailed,
                                 "The requested page strobe clipping references one missing or unsupported primitive.");
        }

        page->strobe = request.strobe;
        RefreshPageBlinkState(*page);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageId.value,
                             "Automation replaced the page strobe definition.");
        return AutomationStatus::Success();
    }

    AutomationStatus ApplyConcreteAction(const DeletePageStrobeRequest& request)
    {
        mfd::PageDefinition* page = FindPageById(context_.bridge.MutableLoaded(), request.pageId, nullptr);
        if (page == nullptr)
        {
            return FailureStatus(AutomationErrorCode::NotFound, "Unknown page id.");
        }
        if (!page->strobe.has_value())
        {
            return FailureStatus(AutomationErrorCode::NotFound, "The requested page has no strobe to delete.");
        }

        page->strobe.reset();
        RefreshPageBlinkState(*page);
        context_.events.Push(AutomationEventKind::DocumentChanged,
                             request.pageId.value,
                             "Automation deleted the page strobe definition.");
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

            if (const std::optional<std::string> templateId = ExtractReticleAssetTemplateId(request.targetId);
                templateId.has_value())
            {
                context_.bridge.SelectReticleAsset(*templateId);
            }
            else
            {
                return FailureStatus(AutomationErrorCode::InvalidArgument, "Reticle asset selection requires one reticle asset id.");
            }
            context_.events.Push(AutomationEventKind::SelectionChanged, request.targetId, "Automation selected one reticle asset.");
            return AutomationStatus::Success();
        case AutomationSelectionKind::Primitive:
            for (const auto& [templateId, reticle] : context_.bridge.Loaded().document.reticleLibrary)
            {
                for (int primitiveIndex = 0; primitiveIndex < static_cast<int>(reticle.primitives.size()); ++primitiveIndex)
                {
                    if (MakeReticleAssetPrimitiveId(templateId,
                                                    reticle.primitives[static_cast<std::size_t>(primitiveIndex)],
                                                    primitiveIndex)
                            .value != request.targetId)
                    {
                        continue;
                    }

                    context_.bridge.SelectReticlePrimitive(templateId, primitiveIndex);
                    context_.events.Push(AutomationEventKind::SelectionChanged,
                                         request.targetId,
                                         "Automation selected one library primitive.");
                    return AutomationStatus::Success();
                }
            }

            return FailureStatus(AutomationErrorCode::NotFound, "Unknown library primitive id.");
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
        if (context_.activeSession.has_value())
        {
            return FailureResult<SaveResult>(AutomationErrorCode::InvalidState,
                                             "Commit or roll back the active preview session before saving.");
        }

        try
        {
            SaveResult result;

            switch (request.kind)
            {
            case SaveAssetRequest::Kind::Page:
            {
                int pageIndex = -1;
                const mfd::PageDefinition* page = FindPageById(context_.bridge.Loaded(), PageId {request.entityId}, &pageIndex);
                if (page == nullptr)
                {
                    return FailureResult<SaveResult>(AutomationErrorCode::NotFound, "Unknown page id.");
                }
                if (pageIndex < 0 || pageIndex >= static_cast<int>(context_.bridge.Files().pageFiles.size()))
                {
                    return FailureResult<SaveResult>(AutomationErrorCode::InvalidState,
                                                     "Missing page file path for the requested page asset.");
                }

                WriteUtf8TextFile(context_.bridge.Loaded().window.sourceFile,
                                  editor::SerializeWindowToJsonString(context_.bridge.Loaded().window,
                                                                      context_.bridge.Loaded().document,
                                                                      context_.bridge.Files()));
                const std::filesystem::path pageFile =
                    context_.bridge.Files().pageFiles[static_cast<std::size_t>(pageIndex)];
                WriteUtf8TextFile(pageFile,
                                  editor::SerializePageToJsonString(*page,
                                                                    context_.bridge.Loaded().document.reticleLibrary,
                                                                    context_.bridge.Files(),
                                                                    static_cast<std::size_t>(pageIndex)));

                result.savedFiles.push_back(context_.bridge.Loaded().window.sourceFile);
                result.savedFiles.push_back(pageFile);
                context_.events.Push(AutomationEventKind::AssetSaved,
                                     request.entityId,
                                     "Automation saved one page asset and refreshed the window manifest.");
                return AutomationResult<SaveResult>::Success(std::move(result));
            }
            case SaveAssetRequest::Kind::ReticleAsset:
            {
                const mfd::ReticleGroup* reticle =
                    FindReticleAssetById(context_.bridge.Loaded(), ReticleAssetId {request.entityId});
                if (reticle == nullptr)
                {
                    return FailureResult<SaveResult>(AutomationErrorCode::NotFound, "Unknown reticle asset id.");
                }

                const auto iterator = context_.bridge.Files().templateFiles.find(reticle->id);
                if (iterator == context_.bridge.Files().templateFiles.end())
                {
                    return FailureResult<SaveResult>(AutomationErrorCode::InvalidState,
                                                     "Missing template file path for the requested reticle asset.");
                }

                WriteUtf8TextFile(iterator->second,
                                  editor::SerializeReticleTemplateToJsonString(*reticle, iterator->second.parent_path()));

                result.savedFiles.push_back(iterator->second);
                context_.events.Push(AutomationEventKind::AssetSaved,
                                     request.entityId,
                                     "Automation saved one reticle asset.");
                return AutomationResult<SaveResult>::Success(std::move(result));
            }
            }
        }
        catch (const std::exception& exception)
        {
            return FailureResult<SaveResult>(AutomationErrorCode::IoFailure, "Save failed: " + std::string(exception.what()));
        }

        return FailureResult<SaveResult>(AutomationErrorCode::Unsupported, "Unsupported save-asset request.");
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
