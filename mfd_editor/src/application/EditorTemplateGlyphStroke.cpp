/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "internal/application/EditorApplicationAuthoringSupport.h"

namespace editor::app
{
namespace
{
void SetGlyphStroke(mfd::Primitive& primitive, const bool enabled, const float thickness)
{
    if (auto* text = std::get_if<mfd::TextGeometry>(&primitive.geometry))
    {
        text->useStrokeThickness = enabled;
        primitive.style.thickness = thickness;
    }
    else if (auto* clock = std::get_if<mfd::TimeGeometry>(&primitive.geometry))
    {
        clock->useStrokeThickness = enabled;
        primitive.style.thickness = thickness;
    }
}

void UpdateInstance(mfd::ReticleGroup& instance, const mfd::ReticleGroup& source,
                    const std::size_t index, const bool enabled, const float thickness)
{
    if (instance.sourceTemplateId != source.id)
    {
        return;
    }
    const auto& authored = source.primitives[index];
    // Unnamed elements have only their position in the template as an identity.
    auto* target = index < instance.primitives.size() ? &instance.primitives[index] : nullptr;
    if (!authored.id.empty() && (target == nullptr || target->id != authored.id))
    {
        target = mfd::FindPrimitive(instance, authored.id);
    }
    if (target != nullptr && target->id == authored.id && target->type == authored.type)
    {
        SetGlyphStroke(*target, enabled, thickness);
    }
}
} // namespace

void ApplyTemplateGlyphStroke(mfd::MfdDocument& document, mfd::ReticleGroup& source,
                             const std::size_t primitiveIndex, const bool enabled, const float thickness)
{
    SetGlyphStroke(source.primitives.at(primitiveIndex), enabled, thickness);
    for (auto& page : document.pages)
    {
        for (auto& instance : page.staticReticles)
        {
            UpdateInstance(instance, source, primitiveIndex, enabled, thickness);
        }
        for (auto& strobe : page.strobes)
        {
            UpdateInstance(strobe.reticle, source, primitiveIndex, enabled, thickness);
        }
    }
}
} // namespace editor::app
