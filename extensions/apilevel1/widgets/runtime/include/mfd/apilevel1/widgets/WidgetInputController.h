#pragma once
#include "mfd/apilevel1/widgets/WidgetRuntimeHost.h"
#include "mfd/apilevel1/widgets/WidgetTypes.h"
#include "mfd/apilevel1/widgets/WidgetCommands.h"
#include <string>
namespace mfd { class SceneRegistry; }
namespace mfd::api1
{
/** @brief Bounded capture, timeout and best-effort feedback recovery for one host viewport. */
class WidgetInputController final
{
public:
    WidgetInputController();
    bool Update(SceneRegistry& scene, const WidgetRuntimeHost& runtime,
        const std::vector<ContainerDefinition>& nodes, const std::vector<ContainerLayoutNode>& layouts,
        const std::vector<WidgetInstance>& widgets, const std::vector<WidgetState>& states,
        Vec2 pointer, bool down, bool began, bool released, bool cancel, float elapsed);
private:
    void Emit(SceneRegistry& scene, TransportId pageId, TransportId widgetId, std::uint64_t requestId,
              WidgetEvent event, bool pressed, bool checked, double value);
    std::string page_;
    std::string captured_;
    WidgetFeedback captureSnapshot_;
    std::uint64_t session_ = 1;
    std::uint64_t sequence_ = 0;
    std::uint64_t request_ = 0;
    float capturedSeconds_ = 0;
    float pendingSeconds_ = 0;
    float snapshotSeconds_ = 0;
    std::size_t snapshotIndex_ = 0;
};
}
