/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

#include "mfd/apilevel1/ContainerEditorModel.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "mfd/apilevel1/ContainerLayoutEngine.h"
#include "mfd/model/PageName.h"
#include "mfd/apilevel1/widgets/WidgetEditorModel.h"

namespace mfd::api1
{
namespace
{
std::string UniqueId(const PageDefinition& page, const std::string_view base)
{
    return WidgetEditorModel::UniqueId(page, base.empty() ? "Container" : base);
}

ContainerDefinition MakeContainer(const PageDefinition& page, const ContainerType type)
{
    ContainerDefinition result;
    result.id = UniqueId(page, "Container");
    result.name = result.id;
    result.type = type;
    result.appearance.borderVisible = true;
    result.appearance.backgroundVisible = true;
    result.appearance.backgroundColor = {12U, 28U, 34U, 110U};
    return result;
}

ContainerDefinition* FindParent(std::vector<ContainerDefinition>& containers, const std::string_view childId)
{
    for (ContainerDefinition& candidate : containers)
    {
        const auto child = std::find_if(candidate.children.begin(), candidate.children.end(),
                                        [childId](const ContainerChildPlacement& placement)
                                        {
                                            return placement.childId == childId;
                                        });
        if (child != candidate.children.end()) return &candidate;
    }
    return nullptr;
}

void RemoveOwnershipEdge(std::vector<ContainerDefinition>& containers, const std::string_view childId)
{
    for (ContainerDefinition& candidate : containers)
    {
        candidate.children.erase(
            std::remove_if(candidate.children.begin(), candidate.children.end(),
                           [childId](const ContainerChildPlacement& placement)
                           {
                               return placement.childId == childId;
                           }),
            candidate.children.end());
    }
}

bool IsDescendant(const std::vector<ContainerDefinition>& containers,
                  const std::string_view ancestorId,
                  const std::string_view candidateId)
{
    const ContainerDefinition* ancestor = FindContainer(containers, ancestorId);
    if (ancestor == nullptr) return false;
    std::vector<std::string> pending;
    pending.reserve(containers.size());
    for (const ContainerChildPlacement& child : ancestor->children)
        if (child.kind == ContainerChildKind::Container) pending.push_back(child.childId);
    for (std::size_t index = 0U; index < pending.size() && index < containers.size(); ++index)
    {
        if (pending[index] == candidateId) return true;
        const ContainerDefinition* current = FindContainer(containers, pending[index]);
        if (current != nullptr)
            for (const ContainerChildPlacement& child : current->children)
                if (child.kind == ContainerChildKind::Container) pending.push_back(child.childId);
    }
    return false;
}
} // namespace

std::string ContainerEditorModel::AddRoot(PageDefinition& page, const ContainerType type)
{
    if (page.containers.size() >= kMaximumContainerCount ||
        static_cast<std::uint32_t>(type) > static_cast<std::uint32_t>(ContainerType::Overlay))
    {
        return {};
    }
    ContainerDefinition created = MakeContainer(page, type);
    const std::string id = created.id;
    page.containers.push_back(std::move(created));
    return id;
}

std::string ContainerEditorModel::AddChild(PageDefinition& page,
                                           const std::string_view parentId,
                                           const ContainerType type,
                                           std::string& error)
{
    ContainerDefinition* parent = FindContainer(page.containers, parentId);
    if (parent == nullptr)
    {
        error = "Select a valid parent container before adding a child.";
        return {};
    }
    if (page.containers.size() >= kMaximumContainerCount)
    {
        error = "The Page has reached the container limit.";
        return {};
    }
    const std::string parentStableId = parent->id;
    ContainerDefinition created = MakeContainer(page, type);
    created.geometry.size = {0.25f, 0.25f};
    created.layoutReferenceSize = created.geometry.size;
    const std::string id = created.id;
    page.containers.push_back(std::move(created));
    FindContainer(page.containers, parentStableId)->children.push_back(ContainerChildPlacement {id});
    if (!ValidatePageWidgets(page.containers, page.widgets, error))
    {
        FindContainer(page.containers, parentStableId)->children.pop_back();
        page.containers.pop_back();
        return {};
    }
    error.clear();
    return id;
}

bool ContainerEditorModel::Rename(PageDefinition& page,
                                  const std::string_view containerId,
                                  std::string newId,
                                  std::string& error)
{
    ContainerDefinition* container = FindContainer(page.containers, containerId);
    if (container == nullptr || PageNameIsBlank(newId))
    {
        error = "Container rename requires an existing container and a non-empty identifier.";
        return false;
    }
    const auto collision = std::find_if(
        page.containers.begin(),
        page.containers.end(),
        [&newId](const ContainerDefinition& candidate)
        {
            return PageNamesEqual(candidate.id, newId);
        });
    if ((collision != page.containers.end() && &*collision != container) ||
        std::any_of(page.widgets.begin(), page.widgets.end(), [&newId](const WidgetInstance& widget) { return PageNamesEqual(widget.id, newId); }))
    {
        error = "Another container already uses identifier '" + newId + "'.";
        return false;
    }
    const std::string previousId = container->id;
    container->id = std::move(newId);
    if (container->name.empty() || container->name == previousId) container->name = container->id;
    for (ContainerDefinition& candidate : page.containers)
        for (ContainerChildPlacement& child : candidate.children)
            if (child.childId == previousId) child.childId = container->id;
    error.clear();
    return true;
}

std::string ContainerEditorModel::Duplicate(PageDefinition& page,
                                            const std::string_view containerId,
                                            std::string& error)
{
    if (!ValidatePageWidgets(page.containers, page.widgets, error)) return {};
    const ContainerDefinition* source = FindContainer(page.containers, containerId);
    if (source == nullptr)
    {
        error = "Select a valid container to duplicate.";
        return {};
    }
    const std::string sourceId = source->id;
    std::vector<std::string> subtree {sourceId};
    for (std::size_t index = 0U; index < subtree.size() && index < page.containers.size() + page.widgets.size(); ++index)
    {
        const ContainerDefinition* current = FindContainer(page.containers, subtree[index]);
        if (current != nullptr)
            for (const ContainerChildPlacement& child : current->children) subtree.push_back(child.childId);
    }
    if (subtree.size() > kMaximumContainerCount + kMaximumWidgetCount)
    {
        error = "Duplicating this subtree would exceed the Page's container limit.";
        return {};
    }
    std::unordered_map<std::string, std::string> renamed;
    renamed.reserve(subtree.size());
    PageDefinition scratch = page;
    for (const std::string& id : subtree)
    {
        const std::string duplicateId = UniqueId(scratch, id + "Copy");
        renamed.emplace(id, duplicateId);
        const auto* sourceContainer = FindContainer(page.containers, id);
        if (sourceContainer)
        {
            ContainerDefinition copy = *sourceContainer;
            copy.id = duplicateId;
            copy.name = duplicateId;
            scratch.containers.push_back(std::move(copy));
        }
        else
        {
            WidgetInstance copy = *WidgetEditorModel::Find(page, id);
            copy.id = duplicateId;
            scratch.widgets.push_back(std::move(copy));
        }
    }
    for (std::size_t index = page.containers.size(); index < scratch.containers.size(); ++index)
        for (ContainerChildPlacement& child : scratch.containers[index].children)
            child.childId = renamed.at(child.childId);

    ContainerDefinition* sourceParent = FindParent(scratch.containers, sourceId);
    if (sourceParent != nullptr)
    {
        const auto sourcePlacement = std::find_if(sourceParent->children.begin(), sourceParent->children.end(),
                                                  [containerId](const ContainerChildPlacement& placement)
                                                  {
                                                      return placement.childId == containerId;
                                                  });
        if (sourcePlacement != sourceParent->children.end())
        {
            ContainerChildPlacement duplicatePlacement = *sourcePlacement;
            duplicatePlacement.childId = renamed.at(sourceId);
            sourceParent->children.push_back(std::move(duplicatePlacement));
        }
    }
    std::string validationError;
    if (!ValidatePageWidgets(scratch.containers, scratch.widgets, validationError))
    {
        error = validationError;
        return {};
    }
    page = std::move(scratch);
    error.clear();
    return renamed.at(sourceId);
}

bool ContainerEditorModel::Delete(PageDefinition& page,
                                  const std::string_view containerId,
                                  std::string& error)
{
    if (FindContainer(page.containers, containerId) == nullptr)
    {
        error = "Select a valid container to delete.";
        return false;
    }
    std::unordered_set<std::string> removed {std::string(containerId)};
    for (std::size_t pass = 0U; pass < page.containers.size(); ++pass)
    {
        const std::size_t before = removed.size();
        for (const ContainerDefinition& candidate : page.containers)
            if (removed.find(candidate.id) != removed.end())
                for (const ContainerChildPlacement& child : candidate.children) removed.insert(child.childId);
        if (removed.size() == before) break;
    }
    page.widgets.erase(std::remove_if(page.widgets.begin(), page.widgets.end(),
        [&removed](const WidgetInstance& widget) { return removed.count(widget.id) != 0U; }), page.widgets.end());
    RemoveOwnershipEdge(page.containers, containerId);
    page.containers.erase(std::remove_if(page.containers.begin(), page.containers.end(),
                                        [&removed](const ContainerDefinition& candidate)
                                        {
                                            return removed.find(candidate.id) != removed.end();
                                        }),
                          page.containers.end());
    error.clear();
    return true;
}

bool ContainerEditorModel::Reparent(PageDefinition& page,
                                    const std::string_view containerId,
                                    const std::string_view newParentId,
                                    std::string& error)
{
    if (FindContainer(page.containers, containerId) == nullptr ||
        (!newParentId.empty() && FindContainer(page.containers, newParentId) == nullptr))
    {
        error = "Container reparenting references an unknown identifier.";
        return false;
    }
    if (containerId == newParentId || IsDescendant(page.containers, containerId, newParentId))
    {
        error = "Container reparenting would create a hierarchy cycle.";
        return false;
    }
    const ContainerDefinition* previousParent = FindParent(page.containers, containerId);
    if ((previousParent == nullptr && newParentId.empty()) ||
        (previousParent != nullptr && previousParent->id == newParentId))
    {
        error.clear();
        return true;
    }
    PageDefinition candidate = page;
    RemoveOwnershipEdge(candidate.containers, containerId);
    if (!newParentId.empty())
        FindContainer(candidate.containers, newParentId)->children.push_back(
            ContainerChildPlacement {std::string(containerId)});
    if (!ValidatePageWidgets(candidate.containers, candidate.widgets, error)) return false;
    page = std::move(candidate);
    error.clear();
    return true;
}
} // namespace mfd::api1
