#include "ExternalStreamUi.h"
#include "mfd/control/CommandClient.h"
#include <memory>

namespace
{
class DemoControl
{
public:
    DemoControl()
    {
        mfd::WindowUdpCommandTransport transport;
        transport.enabled = true; transport.port = 47230;
        client_ = std::make_unique<mfd::CommandClient>(transport);
    }
    bool Apply(const int action)
    {
        auto& first = ui_.Streams().left.Surface();
        auto& second = ui_.Streams().right.Surface();
        switch (action)
        {
        case 0: first.Play(); second.Play(); break;
        case 1: first.Pause(); second.Pause(); break;
        case 2: first.Stop(); second.Stop(); break;
        case 3: first.SetVisible(false); second.SetVisible(false); break;
        case 4: first.SetVisible(true); second.SetVisible(true); break;
        default: return false;
        }
        // Retry the page activation too: a lost first UDP packet must not leave the startup splash visible.
        if (!client_->ActivatePage(ui_.Streams())) return false;
        return client_->SendBatch(ui_.BuildCommandBatch(++sequence_));
    }
private:
    external_stream_demo::ExternalStreamUi ui_;
    std::unique_ptr<mfd::CommandClient> client_;
    std::uint32_t sequence_ = 0;
};
}
extern "C"
{
/** @brief Exercises the generated API against the example window's existing command transport.
 * @param action 0 Play, 1 Pause, 2 Stop, 3 hide, 4 show.
 * @return One when the command was sent; zero on invalid action or transport failure.
 */
__declspec(dllexport) int mfd_demo_control(int action) noexcept
{
    try { static DemoControl control; return control.Apply(action) ? 1 : 0; }
    catch (...) { return 0; }
}
}
