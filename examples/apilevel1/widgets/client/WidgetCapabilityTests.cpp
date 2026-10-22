#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "mfd/runtime_api/RuntimeSession.h"
#include "mfd/apilevel1/widgets/WidgetRuntimeHost.h"
#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
class WidgetCapabilityTest : public testing::Test
{
protected:
    std::filesystem::path folder = std::filesystem::temp_directory_path() /
        ("mfd_widget_capabilities_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path assets = folder / "assets";
    void SetUp() override
    {
        std::filesystem::create_directories(assets);
        std::filesystem::copy(MFD_WIDGET_EXAMPLE_ASSETS, assets, std::filesystem::copy_options::recursive);
        const auto file = assets / "windows/widgets_window.json";
        nlohmann::json window; std::ifstream(file) >> window;
        window.erase("commands"); window.erase("feedback");
        Write(file, window);
        Write(assets / "windows/container_window.json", {
            {"apiLevel",1},{"title","Containers"},{"reticleLibraryFolder","../reticles"},{"pages",{"../pages/containers.json"}}});
        Write(assets / "pages/containers.json", {
            {"name","Containers"},{"layers",{{{"id","main"}}}},{"containers",{{{"id","Parent"}}}}});
    }
    static void Write(const std::filesystem::path& file, const nlohmann::json& value)
    {
        std::ofstream output(file); output << value.dump(2);
        if (!output) throw std::runtime_error("Cannot write capability fixture.");
    }
    void TearDown() override { std::error_code error; std::filesystem::remove_all(folder, error); }
};
}
TEST_F(WidgetCapabilityTest, MissingWidgetModulePreservesLiveWindowThenCompatibleModuleAllowsReload)
{
    ASSERT_FALSE(std::filesystem::exists(std::filesystem::current_path() / "mfd_widgets_api1.dll"));
    ASSERT_FALSE(std::filesystem::exists(std::filesystem::current_path() / "mfd_editor_widgets_api1.dll"));
    mfd::runtime_api::RuntimeSession session;
    std::string error;
    const auto legacy = assets / "windows/container_window.json";
    const auto widgets = assets / "windows/widgets_window.json";
    ASSERT_TRUE(session.LoadWindowFile(legacy, error)) << error;
    EXPECT_FALSE(session.LoadWindowFile(widgets, error));
    EXPECT_NE(error.find("Widgets runtime"), std::string::npos) << error;
    EXPECT_EQ(session.ActivePageName(), "Containers");
    mfd::api1::WidgetRuntimeHost available;
    ASSERT_TRUE(available.Load(MFD_WIDGET_RUNTIME_FILE, error)) << error;
    ASSERT_TRUE(session.LoadWindowFile(widgets, error)) << error;
    EXPECT_EQ(session.ActivePageName(), "Widgets");
    ASSERT_TRUE(session.Reload(error)) << error;
    EXPECT_EQ(session.ActivePageName(), "Widgets");
}
