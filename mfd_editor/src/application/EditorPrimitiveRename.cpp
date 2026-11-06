/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorApplicationAuthoringSupport.h"
#include "mfd/model/RuntimeBudgets.h"

namespace editor::app
{
namespace
{
bool Collides(const mfd::ReticleGroup& group, const mfd::Primitive& target, const std::string& name)
{
    return std::any_of(group.primitives.begin(), group.primitives.end(),
        [&target, &name](const mfd::Primitive& other)
        {
            return &other != &target && mfd::PageNamesEqual(other.id, name);
        });
}

void RenameElement(mfd::ReticleGroup& group, mfd::Primitive& target, const std::string& name)
{
    if (!target.id.empty() && group.clipping.primitiveId == target.id &&
        mfd::FindPrimitive(group, target.id) == &target)
    {
        group.clipping.primitiveId = name;
    }
    target.id = name;
}

bool RenameInstance(mfd::ReticleGroup& instance, const mfd::ReticleGroup& source,
                    std::size_t index, const std::string& name)
{
    if (instance.sourceTemplateId != source.id) return true;
    const auto& authored = source.primitives[index];
    auto* target = index < instance.primitives.size() ? &instance.primitives[index] : nullptr;
    if (!target || target->id != authored.id || target->type != authored.type)
    {
        target = authored.id.empty() ? nullptr : mfd::FindPrimitive(instance, authored.id);
    }
    if (!target) return true;
    if (Collides(instance, *target, name)) return false;
    RenameElement(instance, *target, name);
    return true;
}
} // namespace

bool RenamePrimitiveInCandidate(mfd::MfdDocument& candidate, const std::string& templateId,
                                std::size_t index, const std::string& name)
{
    auto source = candidate.reticleLibrary.find(templateId);
    if (source == candidate.reticleLibrary.end() || index >= source->second.primitives.size() ||
        mfd::PageNameIsBlank(name) || !mfd::runtime_validation::IsValidTextPayload(name)) return false;
    auto& reticle = source->second;
    auto& primitive = reticle.primitives[index];
    if (Collides(reticle, primitive, name)) return false;
    for (auto& page : candidate.pages)
    {
        for (auto& instance : page.staticReticles)
        {
            if (!RenameInstance(instance, reticle, index, name)) return false;
        }
        for (auto& strobe : page.strobes)
        {
            if (!RenameInstance(strobe.reticle, reticle, index, name)) return false;
        }
    }
    RenameElement(reticle, primitive, name);
    return true;
}
} // namespace editor::app
