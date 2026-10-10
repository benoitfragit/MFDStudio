#pragma once
#include <memory>
#include <string>
#include <vector>
#include <raylib.h>
#include "mfd/control/WindowFeedback.h"
#include "mfd/model/Reticle.h"
#include "mfd/runtime/SceneRegistry.h"
#include "mfd/stream/StreamReceiver.h"

namespace mfd
{
/** @brief Renderer-owned resources: four active endpoints, 128 MiB per linked native library. */
class ExternalStreamRegistry
{
public:
    ExternalStreamRegistry();
    void Clear();
    void BeginFrame();
    void Observe(const ReticleGroup& reticle, bool visible);
    void Update();
    void EndFrame();
    Texture2D Resolve(const ExternalStreamGeometry& geometry) const;
    const std::vector<ExternalStreamFeedback>& Feedback() const noexcept { return feedback_; }
    const std::string& Instance() const noexcept { return instance_; }
private:
    struct Entry
    {
        std::string id;
        std::unique_ptr<stream::StreamReceiver> receiver;
        StreamPlayback playback = StreamPlayback::Play;
        std::uint64_t stopRevision = 0;
        std::size_t references = 0;
        bool visible = false;
    };
    std::string instance_;
    std::vector<Entry> entries_;
    std::vector<ExternalStreamFeedback> feedback_;
    bool limitReported_ = false;
};
/** @brief Flushes draw batches before recording GPU fences, including early host returns. */
class ExternalStreamFrame
{
public:
    ExternalStreamFrame(ExternalStreamRegistry& registry, const std::vector<ReticleRenderView>& reticles,
                        bool visible = true);
    ~ExternalStreamFrame();
    ExternalStreamFrame(const ExternalStreamFrame&) = delete;
    ExternalStreamFrame& operator=(const ExternalStreamFrame&) = delete;
private:
    ExternalStreamRegistry& registry_;
};
} // namespace mfd
