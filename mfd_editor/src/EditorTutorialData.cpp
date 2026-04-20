/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorTutorialData.h"

/**
 * @file
 * @brief Tutorial step metadata for validated editor guidance and file review.
 */

#include <array>

namespace editor::tutorial
{
namespace
{
constexpr std::array<TutorialStepDefinition, 20> kTutorialSteps {{
    {TutorialStepKind::UiAction,
     "Create the tutorial window",
     "Build a clean tutorial window from scratch so the rest of the walkthrough starts from a predictable document.",
     "menu_file",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Create the radar-track reticle",
     "Create the shared reticle template used later by the client when it spawns dynamic tutorial tracks.",
     "menu_reticle",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Create the circle reticle",
     "Create a second template that will become the large circular mask on Page1.",
     "menu_reticle",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Create Page1",
     "Author the first page of the tutorial so the runtime has a main working page to display.",
     "menu_page",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Add the circle reticle to Page1",
     "Instantiate the circle template on Page1 so you can immediately see how library templates become page reticles.",
     "library_add_to_page",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Clip the reticle from the outside",
     "Open the clipping context menu on the circle reticle and keep only the outside region to discover page-local masking.",
     "page_preview_clip_source",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Add and hide an editor layer",
     "Create one extra editor-only layer and hide it to understand that authoring layers never affect runtime rendering.",
     "inspector_add_layer",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Create Page2",
     "Add a second page so the tutorial client can demonstrate page switching.",
     "menu_page",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Save the tutorial assets",
     "Write the authored tutorial files to disk before moving to the code-focused part of the walkthrough.",
     "menu_file",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::FileReview,
     "Review the Page1 strobe JSON",
     "This step explains the JSON block that adds a page strobe so the runtime can jump to each newly created track.",
     "",
     "assets/pages/mfd_tutorial_page1.json",
     "{\n"
     "  \"name\": \"Page1\",\n"
     "  \"title\": \"Page 1\",\n"
     "  \"backgroundColor\": [0, 32, 96, 255],\n"
     "  \"staticReticles\": []\n"
     "}",
     "{\n"
     "  \"name\": \"Page1\",\n"
     "  \"title\": \"Page 1\",\n"
     "  \"backgroundColor\": [0, 32, 96, 255],\n"
     "  \"strobe\": {\n"
     "    \"reticle\": {\n"
     "      \"id\": \"tutorial_strobe\",\n"
     "      \"sourceTemplateId\": \"mfd_tutorial_circle\"\n"
     "    },\n"
     "    \"capture\": {\n"
     "      \"shape\": \"circle\"\n"
     "    }\n"
     "  },\n"
     "  \"staticReticles\": []\n"
     "}",
     "The strobe block declares a page-owned reticle and the capture shape used by the runtime when the client moves the strobe over new tracks.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review page switching in the client",
     "The client alternates between Page1 and Page2 on a timer so the user sees both authored pages without manual input.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "constexpr std::string_view kPage1 = \"Page1\";\n"
     "\n"
     "std::string activePage(kPage1);\n"
     "client.ActivatePage(activePage);",
     "constexpr std::string_view kPage1 = \"Page1\";\n"
     "constexpr std::string_view kPage2 = \"Page2\";\n"
     "constexpr auto kPageSwitchInterval = std::chrono::seconds(30);\n"
     "\n"
     "std::string activePage(kPage1);\n"
     "client.ActivatePage(activePage);\n"
     "\n"
     "auto nextPageTime = std::chrono::steady_clock::now() + kPageSwitchInterval;\n"
     "if (now >= nextPageTime)\n"
     "{\n"
     "    activePage = (activePage == kPage1) ? std::string(kPage2) : std::string(kPage1);\n"
     "    client.ActivatePage(activePage);\n"
     "    nextPageTime += kPageSwitchInterval;\n"
     "}",
     "Switching pages in code demonstrates that authored pages stay data-driven while the client decides when each page becomes active.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic reticle creation",
     "The client builds one patch per new track and upserts it from the shared radar-track template.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "const std::string trackId = \"tutorial_track_\" + std::to_string(serial++);\n"
     "mfd::ReticlePatch patch;\n"
     "patch.position = mfd::Vec2 {axis(rng), axis(rng)};",
     "const std::string trackId = \"tutorial_track_\" + std::to_string(serial++);\n"
     "mfd::ReticlePatch patch;\n"
     "patch.position = mfd::Vec2 {axis(rng), axis(rng)};\n"
     "patch.color = mfd::ColorRgba {80, 255, 185, 255};\n"
     "patch.thickness = 0.0038f;\n"
     "patch.text = std::string(\"T\") + std::to_string(serial);\n"
     "client.UpsertDynamicReticle(kPage1, trackId, kTrackTemplate, patch);",
     "Upsert keeps the client logic simple: the same call handles first creation and later refreshes of the same dynamic reticle id.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic reticle removal",
     "When the client reaches the maximum track count it removes the oldest track before adding a new one.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "if (trackIds.size() >= kMaxTracks)\n"
     "{\n"
     "    trackIds.erase(trackIds.begin());\n"
     "}",
     "if (trackIds.size() >= kMaxTracks)\n"
     "{\n"
     "    client.RemoveDynamicReticle(kPage1, trackIds.front());\n"
     "    trackIds.erase(trackIds.begin());\n"
     "}",
     "The explicit remove call keeps the runtime scene bounded and shows how dynamic ids stay under client ownership.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review static reticle commands",
     "Static reticles can still be animated from code without recreating them, which is useful for highlights, declutter and emphasis.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "// Page1 keeps its authored static reticles unchanged.",
     "client.SetReticleColor(kPage1, \"mfd_tutorial_circle\", {0, 255, 0, 255});\n"
     "client.SetReticlePosition(kPage1, \"mfd_tutorial_circle\", {0.0f, 0.0f});\n"
     "client.SetReticleVisible(kPage1, \"mfd_tutorial_circle\", true);",
     "These commands target one authored page reticle directly, which is different from dynamic reticles that are created from a template id plus a runtime-owned instance id.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic reticle updates",
     "Updating a dynamic reticle means sending a new patch with the same runtime id and the new values you want to keep.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "client.UpsertDynamicReticle(kPage1, trackId, kTrackTemplate, patch);",
     "mfd::ReticlePatch updatedPatch = patch;\n"
     "updatedPatch.color = mfd::ColorRgba {255, 196, 64, 255};\n"
     "updatedPatch.position = mfd::Vec2 {0.15f, -0.10f};\n"
     "client.UpsertDynamicReticle(kPage1, trackId, kTrackTemplate, updatedPatch);",
     "Re-using the same id preserves the logical identity of the track while still changing its appearance or position.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic declutter",
     "Declutter is expressed as a visibility toggle on one dynamic template set so the client can hide or restore a whole family of tracks at once.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "client.UpsertDynamicReticle(kPage1, trackId, kTrackTemplate, patch);",
     "bool tracksVisible = true;\n"
     "if ((serial % 5U) == 0U)\n"
     "{\n"
     "    tracksVisible = !tracksVisible;\n"
     "}\n"
     "client.SetDynamicReticleSetVisible(kPage1, kTrackTemplate, tracksVisible);",
     "Template-set visibility is a compact way to declutter the page without deleting runtime objects or rebuilding the authored content.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review generated UI integration",
     "When generated bindings are available the client can build higher-level commands instead of calling the low-level API directly.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "#include \"mfd/control/CommandClient.h\"\n"
     "#include \"mfd/control/FeedbackTransport.h\"\n"
     "#include \"mfd/control/StrobeFeedback.h\"",
     "#if __has_include(\"MfdTutorialMockupUi.h\")\n"
     "#include \"MfdTutorialMockupUi.h\"\n"
     "#define MFD_HAS_GENERATED_TUTORIAL_UI 1\n"
     "using GeneratedTutorialUi = mockup_ui::MfdTutorialMockupUi;\n"
     "#elif __has_include(\"TutorialUi.h\")\n"
     "#include \"TutorialUi.h\"\n"
     "#define MFD_HAS_GENERATED_TUTORIAL_UI 1\n"
     "using GeneratedTutorialUi = tutorial_ui::TutorialUi;\n"
     "#else\n"
     "#define MFD_HAS_GENERATED_TUTORIAL_UI 0\n"
     "#endif",
     "The generated API keeps page names, template names and field setters strongly typed, which reduces drift between authored JSON and client code.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review strobe feedback handling",
     "The client can subscribe to feedback packets and decode them to monitor the runtime strobe state.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "std::unique_ptr<mfd::IExchangeChannel> feedbackChannel;",
     "std::unique_ptr<mfd::IExchangeChannel> feedbackChannel;\n"
     "if (loaded.window.feedbackTransports.udp.has_value())\n"
     "{\n"
     "    feedbackChannel = mfd::CreateFeedbackReceiverChannel(*loaded.window.feedbackTransports.udp);\n"
     "}\n"
     "\n"
     "const auto payload = feedbackChannel->TryReceive();\n"
     "const std::string_view raw(reinterpret_cast<const char*>(payload->data()), payload->size());\n"
     "std::string error;\n"
     "const auto feedback = mfd::DeserializeStrobeStatusFeedback(raw, &error);",
     "Feedback decoding closes the loop between authored strobe behavior in the window and the external application driving it.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review RGBA32 framebuffer capture",
     "The launcher can expose raw RGBA32 pixels so the tutorial can verify what the runtime is rendering.",
     "",
     "examples/mfd_tutorial/src/main.cpp",
     "return mfd::window::RunLauncher(argc, argv, config);",
     "return mfd::window::RunLauncher(\n"
     "    argc,\n"
     "    argv,\n"
     "    config,\n"
     "    [](int width, int height, mfd::ByteView pixels)\n"
     "    {\n"
     "        std::cout << \"RGBA32 framebuffer callback active: \" << width << \"x\" << height\n"
     "                  << \" pixels=\" << pixels.size() << '\\n';\n"
     "    });",
     "This callback is the simplest bridge between the runtime window and any external validation or capture workflow.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review tutorial target registration",
     "The repository root must register the tutorial executables so the authored assets and the client walkthrough can both be built.",
     "",
     "CMakeLists.txt",
     "if(MFD_BUILD_DEMO)\n"
     "{\n"
     "    add_subdirectory(examples/mfd_demo)\n"
     "    add_subdirectory(examples/mfd_demo_cockpit)\n"
     "    add_subdirectory(examples/mfd_demo_minimal)\n"
     "    add_subdirectory(examples/client_mockup)\n"
     "    add_subdirectory(examples/client_mockup_minimal)\n"
     "    add_subdirectory(mfd_editor)\n"
     "endif()",
     "if(MFD_BUILD_DEMO)\n"
     "{\n"
     "    add_subdirectory(examples/mfd_demo)\n"
     "    add_subdirectory(examples/mfd_demo_cockpit)\n"
     "    add_subdirectory(examples/mfd_demo_minimal)\n"
     "    add_subdirectory(examples/client_mockup)\n"
     "    add_subdirectory(examples/client_mockup_minimal)\n"
     "    add_subdirectory(examples/mfd_tutorial)\n"
     "    add_subdirectory(examples/client_tutorial)\n"
     "    add_subdirectory(mfd_editor)\n"
     "endif()",
     "Registering both tutorial targets ensures the authoring walkthrough and the code walkthrough stay buildable from the same root project.",
     "Finish",
     1,
     1},
}};
} // namespace

mfd::ArrayView<const TutorialStepDefinition> Steps() noexcept
{
    return kTutorialSteps;
}

int StepCount() noexcept
{
    return static_cast<int>(kTutorialSteps.size());
}

bool IsUiStep(const TutorialStepDefinition& step) noexcept
{
    return step.kind == TutorialStepKind::UiAction;
}

bool IsFileReviewStep(const TutorialStepDefinition& step) noexcept
{
    return step.kind == TutorialStepKind::FileReview;
}
} // namespace editor::tutorial
