#include "ExternalStreamRegistry.h"
#include <algorithm>
#include <rlgl.h>

namespace mfd
{
ExternalStreamRegistry::ExternalStreamRegistry() : instance_(stream::NewStreamInstance())
{ entries_.reserve(8); feedback_.reserve(4); }
void ExternalStreamRegistry::Clear() { entries_.clear(); feedback_.clear(); }
void ExternalStreamRegistry::BeginFrame()
{
    for (Entry& entry : entries_) { entry.references = 0; entry.visible = false; }
}
void ExternalStreamRegistry::Observe(const ReticleGroup& reticle, const bool visible)
{
    for (const Primitive& primitive : reticle.primitives)
    {
        const auto* geometry = std::get_if<ExternalStreamGeometry>(&primitive.geometry);
        if (!geometry) continue;
        auto position = std::find_if(entries_.begin(), entries_.end(), [geometry](const Entry& entry)
                                    { return entry.id == geometry->streamId; });
        if (position == entries_.end())
        {
            // Four old entries may coexist with four newly observed IDs until Update retires them.
            if (entries_.size() == 8)
            {
                if (!limitReported_) TraceLog(LOG_WARNING, "External stream limit (4) reached");
                limitReported_ = true;
                continue;
            }
            Entry entry;
            entry.id = geometry->streamId;
            entries_.push_back(std::move(entry));
            position = entries_.end() - 1;
        }
        ++position->references;
        position->visible = visible && primitive.style.visible;
        position->playback = geometry->playback;
        position->stopRevision = geometry->stopRevision;
    }
}
void ExternalStreamRegistry::Update()
{
    feedback_.clear();
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [](const Entry& entry)
                                  { return entry.references == 0; }), entries_.end());
    if (entries_.size() > 4)
    {
        if (!limitReported_) TraceLog(LOG_WARNING, "External stream limit (4) reached; additional streams use their replacement color");
        limitReported_ = true;
        entries_.resize(4);
    }
    else if (entries_.size() < 4) limitReported_ = false;
    for (Entry& entry : entries_)
    {
        if (!entry.receiver)
        {
            entry.receiver = std::make_unique<stream::StreamReceiver>(instance_, entry.id);
            TraceLog(LOG_INFO, "External stream: instance=%s stream=%s", instance_.c_str(), entry.id.c_str());
        }
        entry.receiver->Update(entry.playback, entry.stopRevision, entry.visible && entry.references == 1);
        ExternalStreamFeedback feedback;
        feedback.streamId = entry.id; feedback.instance = instance_; feedback.status = entry.receiver->Status();
        if (entry.references > 1)
        {
            feedback.status.result = StreamResult::Error;
            feedback.status.hasImage = false;
            feedback.status.diagnostic = "Duplicate streamId: one consumer per stream required";
        }
        feedback_.push_back(std::move(feedback));
    }
}
void ExternalStreamRegistry::EndFrame()
{
    if (entries_.empty()) return;
    rlDrawRenderBatchActive();
    for (Entry& entry : entries_)
        if (entry.visible && entry.references == 1) entry.receiver->EndFrame();
}
Texture2D ExternalStreamRegistry::Resolve(const ExternalStreamGeometry& geometry) const
{
    for (const Entry& entry : entries_)
        if (entry.id == geometry.streamId && entry.references == 1)
            return Texture2D {entry.receiver->Texture(), static_cast<int>(entry.receiver->Width()),
                              static_cast<int>(entry.receiver->Height()), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    return {};
}
ExternalStreamFrame::ExternalStreamFrame(ExternalStreamRegistry& registry,
                                       const std::vector<ReticleRenderView>& reticles, const bool visible)
    : registry_(registry)
{
    registry_.BeginFrame();
    for (const auto& reticle : reticles)
        if (reticle.group) registry_.Observe(*reticle.group, visible && reticle.visible);
    registry_.Update();
}
ExternalStreamFrame::~ExternalStreamFrame() { registry_.EndFrame(); }
} // namespace mfd
