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
#include <cstddef>

namespace editor::tutorial
{
namespace
{
constexpr std::array<TutorialStepDefinition, static_cast<std::size_t>(TutorialStepId::Count)> kTutorialSteps {{
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
     "Create the strobe cursor reticle",
     "Create a small cross-shaped reticle from one horizontal line, then append a vertical line so Page1 can later use it as a strobe cursor.",
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
     "Assign the Page1 strobe template",
     "On Page1, choose the small cross cursor as the page strobe so the runtime and the client both target a real authored strobe.",
     "page_strobe_template",
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
     "Create the progress-bar reticle",
     "Create a rectangle-based reticle for Page2, then append a second rectangle so the tutorial can animate a framed progress bar.",
     "menu_reticle",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Expose the progress fill primitive",
     "Select the filling rectangle and mark it as exposed so the generated client API can resize and move it at runtime.",
     "primitive_exposed_checkbox",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Add the progress bar to Page2",
     "Instantiate the progress-bar template on Page2 so the tutorial client can drive it through the generated page bindings.",
     "library_add_to_page",
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
     "This step reviews the JSON block written by the editor when Page1 was configured with its strobe cursor.",
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
     "    \"id\": \"tutorial_strobe\",\n"
     "    \"template\": \"mfd_tutorial_strobe_cursor\",\n"
     "    \"capture\": {\n"
     "      \"shape\": \"circle\",\n"
     "      \"radius\": 0.0875\n"
     "    }\n"
     "  },\n"
     "  \"staticReticles\": []\n"
     "}",
     "The strobe block now references the small cross cursor authored earlier in the editor, and still declares the capture shape used by the runtime when the client moves the strobe over new tracks. Magnetized visual shape changes remain opt-in through the strobe magnet visual settings.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review page switching in the client",
     "The client alternates between Page1 and Page2 on a timer by passing generated page handles to CommandClient. The client extracts the generated page id and mapping hash internally, so the tutorial code does not send page names.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "tutorial_ui::TutorialUi generatedUi;\n"
     "auto& page1 = generatedUi.Page1();\n"
     "\n"
     "if (!client.ActivatePage(page1))\n"
     "{\n"
     "    throw std::runtime_error(\"Unable to activate tutorial Page1: \" + client.LastError());\n"
     "}",
     "tutorial_ui::TutorialUi generatedUi;\n"
     "auto& page1 = generatedUi.Page1();\n"
     "auto& page2 = generatedUi.Page2();\n"
     "constexpr auto kPageSwitchInterval = std::chrono::seconds(30);\n"
     "\n"
     "bool page1Active = true;\n"
     "if (!client.ActivatePage(page1))\n"
     "{\n"
     "    throw std::runtime_error(\"Unable to activate tutorial Page1: \" + client.LastError());\n"
     "}\n"
     "\n"
     "auto nextPageTime = std::chrono::steady_clock::now() + kPageSwitchInterval;\n"
     "if (now >= nextPageTime)\n"
     "{\n"
     "    page1Active = !page1Active;\n"
     "    const bool activated = page1Active ? client.ActivatePage(page1) : client.ActivatePage(page2);\n"
     "    if (!activated)\n"
     "    {\n"
     "        throw std::runtime_error(\"Unable to activate generated tutorial page: \" + client.LastError());\n"
     "    }\n"
     "    nextPageTime += kPageSwitchInterval;\n"
     "}",
     "Switching pages in code demonstrates that authored pages stay data-driven while generated page wrappers keep transport ids and mapping hashes out of application code.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic reticle creation",
     "The tutorial client now creates one generated dynamic handle per track and configures it through typed setters plus one authored primitive accessor.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "const std::uint32_t trackSerial = serial++;\n"
     "const mfd::Vec2 trackPosition {axis(rng), axis(rng)};",
     "const std::uint32_t trackSerial = serial++;\n"
     "const mfd::Vec2 trackPosition {axis(rng), axis(rng)};\n"
     "const float trackSize = 0.18f + 0.01f * static_cast<float>(trackSerial % 3U);\n"
     "\n"
     "auto& generatedTrack = generatedDynamicTracks.Create();\n"
     "generatedTrack.SetPosition(trackPosition);\n"
     "generatedTrack.SetColor({80, 255, 185, 255});\n"
     "generatedTrack.SetThickness(0.0038f);\n"
     "generatedTrack.Primitive01().SetSize({trackSize, trackSize});\n"
     "generatedTrack.Primitive01().SetRotationDegrees(static_cast<float>((trackSerial % 8U) * 12U));",
     "The generated set now owns the hidden runtime id. The tutorial only keeps the typed handle returned by `Create()` and drives the authored primitive through `Primitive01()`.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic reticle removal",
     "When the client reaches the maximum track count it removes the oldest generated handle before adding a new track.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "if (generatedTracks.size() >= kMaxTracks)\n"
     "{\n"
     "    generatedTracks.erase(generatedTracks.begin());\n"
     "}",
     "if (generatedTracks.size() >= kMaxTracks)\n"
     "{\n"
     "    generatedDynamicTracks.Remove(*generatedTracks.front());\n"
     "    generatedTracks.erase(generatedTracks.begin());\n"
     "}",
     "The generated API still removes the runtime object explicitly, but the application never handles an MFD reticle id itself.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review static reticle commands",
     "Static reticles are now addressed through generated page -> reticle -> primitive navigation instead of raw page/reticle strings.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "// Page1 keeps its authored static reticles unchanged.",
     "page1Circle.SetVisible(true);\n"
     "page1Circle.SetColor(\n"
     "    generatedDeclutterVisible ? mfd::ColorRgba {0, 255, 128, 255} : mfd::ColorRgba {0, 96, 48, 255});\n"
     "page1Circle.Primitive01().SetRadius(0.42f + 0.015f * static_cast<float>(generatedTracks.size() + 1U));\n"
     "page1Circle.Primitive01().SetThickness(0.0045f);\n"
     "page2ProgressBar.SetVisible(true);\n"
     "progressFill.SetSize({progressWidth, 0.06f});\n"
     "progressFill.SetPosition({progressCenterX, 0.0f});",
     "The tutorial now animates authored static reticles through generated accessors on both pages, which mirrors the target `page -> reticle -> primitive` navigation exactly.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic reticle updates",
     "One generated dynamic handle can accumulate several field updates before the batch is emitted.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "auto& generatedTrack = generatedDynamicTracks.Create();\n"
     "generatedTrack.SetPosition(trackPosition);",
     "auto& generatedTrack = generatedDynamicTracks.Create();\n"
     "generatedTrack.SetPosition(trackPosition);\n"
     "generatedTrack.SetColor({80, 255, 185, 255});\n"
     "generatedTrack.SetThickness(0.0038f);\n"
     "generatedTrack.Primitive01().SetSize({trackSize, trackSize});\n"
     "generatedTrack.Primitive01().SetRotationDegrees(static_cast<float>((trackSerial % 8U) * 12U));",
     "Those typed setters build one coherent delta for the generated object before `BuildBatch()` serializes it.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review dynamic declutter",
     "Declutter is expressed as a visibility toggle on one dynamic template set so the client can hide or restore a whole family of tracks at once.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "auto& generatedTrack = generatedDynamicTracks.Create();\n"
     "generatedTrack.Primitive01().SetRotationDegrees(static_cast<float>((trackSerial % 8U) * 12U));",
     "bool generatedDeclutterVisible = true;\n"
     "if ((trackSerial % 5U) == 0U)\n"
     "{\n"
     "    generatedDeclutterVisible = !generatedDeclutterVisible;\n"
     "}\n"
     "generatedDynamicTracks.SetVisible(generatedDeclutterVisible);\n"
     "const auto commands = generatedUi.BuildBatch();",
     "Template-set visibility remains a compact way to declutter the page, and the generated batch builder keeps that toggle synchronized with the rest of the authored fields.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review generated UI integration",
     "The tutorial client should consume the generated bindings directly so higher-level commands stay aligned with the authored JSON.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "#include \"mfd/control/CommandClient.h\"\n"
     "#include \"mfd/control/FeedbackTransport.h\"\n"
     "#include \"mfd/control/StrobeFeedback.h\"",
     "#include \"TutorialUi.h\"\n"
     "\n"
     "tutorial_ui::TutorialUi generatedUi;\n"
     "auto& page1 = generatedUi.Page1();\n"
     "auto& page2 = generatedUi.Page2();\n"
     "auto& generatedDynamicTracks = page1.DynamicMfdTutorialRadarTrack();\n"
     "auto& page1Circle = page1.mfdTutorialCircle;\n"
     "auto& page2ProgressBar = page2.mfdTutorialProgressBar;\n"
     "auto& progressFill = page2ProgressBar.FillBar();\n"
     "auto& page1Strobe = page1.strobe;",
     "The generated API now exposes one typed dynamic accessor, two static reticle handles, one exposed primitive accessor, and one generic page `strobe` handle without asking the user to manage ids.",
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
     "The launcher can expose raw RGBA32 pixels through a plugin callback so the tutorial can verify what the runtime is rendering.",
     "",
     "examples/mfd_framebuffer_stdout_plugin/src/FramebufferStdoutPlugin.cpp",
     "extern \"C\" __declspec(dllexport) void MfdWindowFramebufferCallback(\n"
     "    const int width,\n"
     "    const int height,\n"
     "    const mfd::ByteView pixels)\n"
     "{\n"
     "}",
     "extern \"C\" __declspec(dllexport) void MfdWindowFramebufferCallback(\n"
     "    const int width,\n"
     "    const int height,\n"
     "    const mfd::ByteView pixels)\n"
     "{\n"
     "    static bool printed = false;\n"
     "    if (!printed)\n"
     "    {\n"
     "        std::cout << \"RGBA32 framebuffer callback active: \" << width << \"x\" << height\n"
     "                  << \" pixels=\" << pixels.size() << '\\n';\n"
     "        printed = true;\n"
     "    }\n"
     "}",
     "The framebuffer callback now lives in a dedicated DLL consumed by `mfd_window --framebuffer-plugin`, which keeps the generic launcher reusable.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review tutorial target registration",
     "The repository root only needs to register the tutorial client because the authored tutorial window is now launched through `mfd_window` and `Start-MfdTutorial.bat`.",
     "",
     "CMakeLists.txt",
     "if(MFD_BUILD_DEMO)\n"
     "{\n"
     "    add_subdirectory(examples/mfd_framebuffer_stdout_plugin)\n"
     "    add_subdirectory(examples/client_mockup)\n"
     "    add_subdirectory(examples/client_mockup_minimal)\n"
     "    add_subdirectory(mfd_editor)\n"
     "endif()",
     "if(MFD_BUILD_DEMO)\n"
     "{\n"
     "    add_subdirectory(examples/mfd_framebuffer_stdout_plugin)\n"
     "    add_subdirectory(examples/client_mockup)\n"
     "    add_subdirectory(examples/client_mockup_minimal)\n"
     "    add_subdirectory(examples/client_tutorial)\n"
     "    add_subdirectory(mfd_editor)\n"
     "endif()",
     "Registering `client_tutorial` is enough to build the generated API walkthrough, while `Start-MfdTutorial.bat` reuses the shared window host for the authored runtime.",
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
