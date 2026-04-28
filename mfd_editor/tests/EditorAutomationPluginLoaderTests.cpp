/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for the hidden in-process automation plugin loader.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>

#include "EditorAutomationPluginLoader.h"
#include "EditorAutomationServices.h"
#include "EditorAutomationTestUtils.h"

#if defined(_WIN32)
#    include <Windows.h>
#endif

namespace
{
#if defined(_WIN32)
std::filesystem::path TestBinaryDirectory()
{
    std::wstring buffer(static_cast<std::size_t>(MAX_PATH), L'\0');
    DWORD size = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (size == buffer.size())
    {
        buffer.resize(buffer.size() * 2U);
        size = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    buffer.resize(size);
    return std::filesystem::path(buffer).parent_path();
}
#endif

std::filesystem::path TestPluginFile(const std::string_view pluginStem)
{
#if defined(_WIN32)
    return TestBinaryDirectory() / (std::string(pluginStem) + ".dll");
#else
    return std::filesystem::current_path() / std::string(pluginStem);
#endif
}
} // namespace

TEST(EditorAutomationPluginLoaderTests, RejectsEmptyPluginPath)
{
    FakeEditorAutomationBridge bridge;
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    EXPECT_FALSE(loader.Load({}, *facade, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(loader.IsLoaded());
}

TEST(EditorAutomationPluginLoaderTests, RejectsMissingPluginFile)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    EXPECT_FALSE(loader.Load(TestPluginFile("missing_editor_automation_plugin"), *facade, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(loader.IsLoaded());
}

#if defined(_WIN32)
TEST(EditorAutomationPluginLoaderTests, LoadsPluginAndLetsItMutateTheDocument)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    ASSERT_TRUE(loader.Load(TestPluginFile("mfd_editor_test_automation_plugin"), *facade, error)) << error;
    EXPECT_TRUE(loader.IsLoaded());
    EXPECT_EQ(loader.PluginDisplayName(), "MFD Editor Test Automation Plugin");
    EXPECT_EQ(loader.PluginFile().filename(), "mfd_editor_test_automation_plugin.dll");

    loader.Tick();
    ASSERT_EQ(bridge.loaded_.document.pages.size(), 2U);
    EXPECT_EQ(bridge.loaded_.document.pages.back().name, "PluginPage");
    EXPECT_EQ(bridge.loaded_.document.pages.back().title, "Plugin Page");
    ASSERT_EQ(bridge.loaded_.document.pages.back().staticReticles.size(), 1U);
    EXPECT_EQ(bridge.loaded_.document.pages.back().staticReticles.front().id, "plugin_track");
    EXPECT_FALSE(bridge.loaded_.document.pages.back().staticReticles.front().visible);
    EXPECT_TRUE(bridge.loaded_.document.pages.back().staticReticles.front().drawOnTop);
    EXPECT_EQ(bridge.loaded_.document.pages.back().staticReticles.front().editor.layerId, "plugin_overlay");
    ASSERT_EQ(bridge.loaded_.document.pages.back().editor.layers.size(), 2U);
    EXPECT_EQ(bridge.loaded_.document.pages.back().editor.layers.back().id, "plugin_overlay");
    EXPECT_TRUE(bridge.loaded_.document.pages.back().editor.layers.back().visible);
    ASSERT_EQ(bridge.files_.pageFiles.size(), 2U);
    EXPECT_EQ(bridge.files_.pageFiles.back().filename().string(), "plugin_page.json");
    EXPECT_TRUE(std::filesystem::exists(bridge.files_.pageFiles.back()));
    EXPECT_EQ(bridge.pushedUndoSnapshots_.size(), 1U);
    EXPECT_TRUE(loader.LastRuntimeError().empty());

    loader.Tick();
    EXPECT_EQ(bridge.loaded_.document.pages.size(), 2U);

    loader.Unload();
    EXPECT_FALSE(loader.IsLoaded());
    EXPECT_TRUE(loader.PluginDisplayName().empty());
    EXPECT_TRUE(loader.PluginFile().empty());
}

TEST(EditorAutomationPluginLoaderTests, CapturesRuntimeErrorsReportedByPlugins)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    ASSERT_TRUE(loader.Load(TestPluginFile("mfd_editor_test_failing_plugin"), *facade, error)) << error;
    loader.Tick();

    EXPECT_EQ(loader.LastRuntimeError(), "Runtime failure from test plugin");
    EXPECT_TRUE(loader.IsLoaded());

    loader.Unload();
    EXPECT_TRUE(loader.LastRuntimeError().empty());
}

TEST(EditorAutomationPluginLoaderTests, RollsBackOpenPreviewSessionsWhenThePluginIsUnloaded)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    ASSERT_TRUE(loader.Load(TestPluginFile("mfd_editor_test_rollback_on_unload_plugin"), *facade, error)) << error;

    loader.Tick();
    ASSERT_EQ(bridge.loaded_.document.pages.size(), 2U);
    EXPECT_EQ(bridge.loaded_.document.pages.back().name, "TransientPluginPage");
    EXPECT_EQ(bridge.pushedUndoSnapshots_.size(), 0U);

    loader.Unload();
    EXPECT_EQ(bridge.loaded_.document.pages.size(), 1U);
    EXPECT_EQ(bridge.loaded_.document.pages.front().name, "Radar");
    EXPECT_EQ(bridge.pushedUndoSnapshots_.size(), 0U);
}

TEST(EditorAutomationPluginLoaderTests, LetsThePluginTriggerHostOwnedSaveAll)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    ASSERT_TRUE(loader.Load(TestPluginFile("mfd_editor_test_save_all_plugin"), *facade, error)) << error;

    loader.Tick();
    ASSERT_EQ(bridge.loaded_.document.pages.size(), 2U);
    EXPECT_TRUE(loader.LastRuntimeError().empty());
    EXPECT_TRUE(std::filesystem::exists(bridge.windowFile_));
    ASSERT_EQ(bridge.files_.pageFiles.size(), 2U);
    EXPECT_TRUE(std::filesystem::exists(bridge.files_.pageFiles[0]));
    EXPECT_TRUE(std::filesystem::exists(bridge.files_.pageFiles[1]));
    const auto templateIterator = bridge.files_.templateFiles.find("track_box");
    ASSERT_NE(templateIterator, bridge.files_.templateFiles.end());
    EXPECT_TRUE(std::filesystem::exists(templateIterator->second));

    loader.Unload();
}

TEST(EditorAutomationPluginLoaderTests, LoadsPluginAndCoversTheExtendedAutomationSurface)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    ASSERT_TRUE(loader.Load(TestPluginFile("mfd_editor_test_extended_automation_plugin"), *facade, error)) << error;
    EXPECT_TRUE(loader.IsLoaded());
    EXPECT_EQ(loader.PluginDisplayName(), "MFD Editor Extended Automation Plugin");

    loader.Tick();
    EXPECT_TRUE(loader.LastRuntimeError().empty()) << loader.LastRuntimeError();

    ASSERT_EQ(bridge.loaded_.document.pages.size(), 1U);
    EXPECT_EQ(bridge.loaded_.document.pages.front().name, "Radar");
    EXPECT_TRUE(bridge.loaded_.document.pages.front().blinkTypes.empty());
    EXPECT_FALSE(bridge.loaded_.document.pages.front().strobe.has_value());
    EXPECT_TRUE(bridge.loaded_.document.pages.front().staticReticles.empty());
    ASSERT_EQ(bridge.loaded_.document.pages.front().editor.layers.size(), 1U);
    EXPECT_EQ(bridge.loaded_.document.pages.front().editor.layers.front().id, "layer");

    ASSERT_EQ(bridge.loaded_.document.reticleLibrary.size(), 1U);
    EXPECT_NE(bridge.loaded_.document.reticleLibrary.find("track_box"), bridge.loaded_.document.reticleLibrary.end());
    EXPECT_EQ(bridge.files_.pageFiles.size(), 1U);
    EXPECT_EQ(bridge.files_.removedPageFiles.size(), 1U);
    EXPECT_EQ(bridge.files_.templateFiles.size(), 1U);
    EXPECT_EQ(bridge.files_.removedTemplateFiles.size(), 1U);
    EXPECT_EQ(bridge.pushedUndoSnapshots_.size(), 1U);

    loader.Unload();
}

TEST(EditorAutomationPluginLoaderTests, AcceptsStatelessPluginsWithNullPluginContext)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    ASSERT_TRUE(loader.Load(TestPluginFile("mfd_editor_test_stateless_plugin"), *facade, error)) << error;
    EXPECT_TRUE(loader.IsLoaded());
    EXPECT_EQ(loader.PluginDisplayName(), "MFD Editor Stateless Automation Plugin");
    EXPECT_TRUE(loader.LastRuntimeError().empty());

    loader.Tick();
    EXPECT_TRUE(loader.LastRuntimeError().empty());

    loader.Unload();
    EXPECT_FALSE(loader.IsLoaded());
}

TEST(EditorAutomationPluginLoaderTests, RejectsPluginsWithOneUnexpectedAbiVersion)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    EXPECT_FALSE(loader.Load(TestPluginFile("mfd_editor_test_bad_abi_plugin"), *facade, error));
    EXPECT_NE(error.find("ABI version"), std::string::npos);
    EXPECT_FALSE(loader.IsLoaded());
}

TEST(EditorAutomationPluginLoaderTests, SurfacesExplicitStartFailures)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    EXPECT_FALSE(loader.Load(TestPluginFile("mfd_editor_test_start_failure_plugin"), *facade, error));
    EXPECT_EQ(error, "Start failure from test plugin");
    EXPECT_FALSE(loader.IsLoaded());
}

TEST(EditorAutomationPluginLoaderTests, RejectsIncompleteStableAbiDescriptors)
{
    FakeEditorAutomationBridge bridge = MakeSeedEditorAutomationBridge();
    auto facade = editor::automation::CreateEditorAutomationFacade(bridge);
    editor::EditorAutomationPluginLoader loader;
    std::string error;

    EXPECT_FALSE(loader.Load(TestPluginFile("mfd_editor_test_incomplete_plugin"), *facade, error));
    EXPECT_NE(error.find("incomplete callback table"), std::string::npos);
    EXPECT_FALSE(loader.IsLoaded());
}
#endif
