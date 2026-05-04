/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorTutorialData.h"

/**
 * @file
 * @brief Tutorial step metadata for the guided editor walkthrough.
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
     "Allow the tutorial dynamic reticle on Page1",
     "On Page1, enable `mfd_tutorial_radar_track` in the generated dynamic-template list so the generated client can create runtime tutorial tracks.",
     "page_dynamic_template_mfd_tutorial_radar_track",
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
     "Open the page context",
     "Open the page-preview View menu and enable Page context so you can compare the active page with the rest of the window without changing authored JSON.",
     "page_preview_view_menu",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Enable the layer inspector",
     "Open the page-preview View menu and enable Layer Inspector to inspect editor-only layers with their thumbnails and focus strip.",
     "page_preview_view_menu",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Enable the minimap",
     "Open the page-preview View menu and enable Minimap so navigation stays readable on larger authored pages.",
     "page_preview_view_menu",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Highlight reticle usages",
     "Keep one shared reticle template selected, open the page-preview View menu, and enable Highlight reticle usages to see where that template is reused.",
     "page_preview_view_menu",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Show the problems panel",
     "Open the page-preview View menu and enable Problems so validation diagnostics stay docked under the preview.",
     "page_preview_view_menu",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Try fullscreen preview",
     "Enter fullscreen preview, then leave it again, so you can focus on the page canvas without the rest of the editor layout.",
     "page_preview_fullscreen",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Save the tutorial assets",
     "Write the authored tutorial window, pages, and reticles to disk once the guided editor tour is complete.",
     "menu_file",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Inspect page import",
     "Open the page import entry point from the editor menu so you know where shared page ingestion starts. You can cancel the native file dialog after locating it.",
     "menu_page",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Inspect page rename",
     "Open the global page rename workflow, review the scanned references, then close the popup without executing the rename.",
     "menu_page",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Inspect reticle rename",
     "Open the global reticle rename workflow, review the shared template references, then close the popup without executing the rename.",
     "menu_reticle",
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     0,
     0},
    {TutorialStepKind::UiAction,
     "Inspect design export",
     "Open the design export workflow, review the export options and destination, then close the popup without exporting.",
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
     "  \"_editor\": {\n"
     "    \"dynamicReticleTemplates\": [\n"
     "      \"mfd_tutorial_radar_track\"\n"
     "    ]\n"
     "  },\n"
     "  \"staticReticles\": []\n"
     "}",
     "{\n"
     "  \"name\": \"Page1\",\n"
     "  \"title\": \"Page 1\",\n"
     "  \"backgroundColor\": [0, 32, 96, 255],\n"
     "  \"_editor\": {\n"
     "    \"dynamicReticleTemplates\": [\n"
     "      \"mfd_tutorial_radar_track\"\n"
     "    ]\n"
     "  },\n"
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
     "The page now keeps the tutorial radar-track template in its generated dynamic list, while the strobe block references the small cross cursor authored earlier in the editor and still declares the capture shape used by the runtime when the client moves the strobe over new tracks. Magnetized visual shape changes remain opt-in through the strobe magnet visual settings.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review the generated UI header",
     "Open the generated C++17 surface to see how the authored window becomes typed page, reticle, primitive, strobe, and batch helpers.",
     "",
     "examples/client_tutorial/generated/TutorialUi.h",
     "class TutorialUi\n"
     "{\n"
     "public:\n"
     "    std::vector<mfd::UserCommand> BuildBatch();\n"
     "};",
     "class Page1Page\n"
     "{\n"
     "public:\n"
     "    MfdTutorialRadarTrackDynamicReticleSet& DynamicMfdTutorialRadarTrack() noexcept;\n"
     "    StrobeHandle strobe;\n"
     "    Page1MfdTutorialCircleReticle mfdTutorialCircle;\n"
     "};\n"
     "\n"
     "class Page2Page\n"
     "{\n"
     "public:\n"
     "    Page2MfdTutorialProgressBarReticle mfdTutorialProgressBar;\n"
     "};\n"
     "\n"
     "class TutorialUi\n"
     "{\n"
     "public:\n"
     "    Page1Page& Page1() noexcept;\n"
     "    Page2Page& Page2() noexcept;\n"
     "    std::size_t PollFeedback(mfd::IExchangeChannel& channel, std::size_t maxMessages = 64, std::string* error = nullptr);\n"
     "    std::vector<mfd::UserCommand> BuildBatch();\n"
     "    mfd::CommandBatch BuildCommandBatch(std::uint32_t sequence = 0);\n"
     "};",
     "The generated header follows a clear page -> reticle -> primitive composition model. User code stays in typed C++17 wrappers and avoids raw page names, reticle ids, or ad-hoc transport tables.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review the generated transport map",
     "The generated API is paired with one companion transport map that keeps authored names aligned with transport ids and the compatibility hash.",
     "",
     "assets/windows/mfd_tutorial.generated.map",
     "{\n"
     "  \"window\": {\n"
     "    \"source\": \"mfd_tutorial.json\"\n"
     "  }\n"
     "}",
     "{\n"
     "  \"schemaVersion\": 1,\n"
     "  \"mappingHash\": \"tutorial-generated-hash\",\n"
     "  \"window\": {\n"
     "    \"name\": \"mfd_tutorial\",\n"
     "    \"source\": \"mfd_tutorial.json\"\n"
     "  },\n"
     "  \"pages\": [\n"
     "    {\n"
     "      \"name\": \"Page1\",\n"
     "      \"transportId\": 11,\n"
     "      \"reticles\": [\n"
     "        {\n"
     "          \"name\": \"mfdTutorialCircle\",\n"
     "          \"transportId\": 22,\n"
     "          \"primitives\": [\n"
     "            { \"name\": \"primitive_1\", \"transportId\": 33 }\n"
     "          ]\n"
     "        }\n"
     "      ]\n"
     "    }\n"
     "  ]\n"
     "}",
     "The map keeps the runtime wire contract explicit: page, reticle, and primitive ids are generated once, and `mappingHash` lets the client detect mismatches. Dynamic runtime reticles still keep their transient ids hidden behind the generated sets.",
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
     "The generated API now exposes one typed dynamic accessor, two static reticle handles, one exposed primitive accessor, one generic page `strobe` handle, and feedback-backed `IsActive()` / `IsStrobeCaptured()` queries without asking the user to manage ids.",
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
     "generatedTrack.Primitive01().SetRotationDegrees(static_cast<float>((trackSerial % 8U) * 12U));\n"
     "generatedTrack.Primitive01().SetLineStyle(\n"
     "    (trackSerial % 2U) == 0U ? tutorial_ui::LineStyle::Dashed : tutorial_ui::LineStyle::Dotted);",
     "The generated set now owns the hidden runtime id. The tutorial only keeps the typed handle returned by `Create()` and drives the authored primitive through `Primitive01()`, including its generated `LineStyle` enum.",
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
     "generatedTrack.Primitive01().SetRotationDegrees(static_cast<float>((trackSerial % 8U) * 12U));\n"
     "generatedTrack.Primitive01().SetLineStyle(\n"
     "    (trackSerial % 2U) == 0U ? tutorial_ui::LineStyle::Dashed : tutorial_ui::LineStyle::Dotted);",
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
     "page1Circle.Primitive01().SetLineStyle(\n"
     "    generatedDeclutterVisible ? tutorial_ui::LineStyle::Solid : tutorial_ui::LineStyle::Dotted);\n"
     "page2ProgressBar.SetVisible(true);\n"
     "progressFill.SetSize({progressWidth, 0.06f});\n"
     "progressFill.SetPosition({progressCenterX, 0.0f});",
     "The tutorial now animates authored static reticles through generated accessors on both pages, which mirrors the target `page -> reticle -> primitive` navigation exactly.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review runtime feedback handling",
     "The client can subscribe to runtime feedback packets and consume them directly through the generated Page1 API.",
     "",
     "examples/client_tutorial/src/main.cpp",
     "std::unique_ptr<mfd::IExchangeChannel> feedbackChannel;",
     "std::unique_ptr<mfd::IExchangeChannel> feedbackChannel;\n"
     "if (loaded.window.feedbackTransports.udp.has_value())\n"
     "{\n"
     "    feedbackChannel = mfd::CreateFeedbackReceiverChannel(*loaded.window.feedbackTransports.udp);\n"
     "}\n"
     "\n"
     "std::string feedbackError;\n"
     "const std::size_t appliedFeedbackCount = generatedUi.PollFeedback(*feedbackChannel, 8U, &feedbackError);\n"
     "const bool page1Active = page1.IsActive();\n"
     "const bool trackCaptured = !generatedTracks.empty() && generatedTracks.back()->IsStrobeCaptured();",
     "Generated feedback polling closes the loop between authored strobe behavior in the window and the external application, while keeping `Page1.IsActive()` and dynamic-track capture state available directly on the typed handles.",
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
     "Review the tutorial build gate",
     "The tutorial client stays out of the default examples build until the integrated walkthrough completes and registers it automatically.",
     "",
     "examples/client_tutorial/CMakeLists.txt",
     "client_api_generate_ui(\n"
     "    WINDOW_JSON \"${MFD_ROOT_DIR}/assets/windows/mfd_tutorial.json\"\n"
     "    OUTPUT_HEADER \"${MFD_CLIENT_TUTORIAL_GENERATED_HEADER}\"\n"
     "    OUTPUT_SOURCE \"${MFD_CLIENT_TUTORIAL_GENERATED_SOURCE}\")",
     "set(MFD_CLIENT_TUTORIAL_REQUIRED_FILES\n"
     "    \"${MFD_ROOT_DIR}/assets/windows/mfd_tutorial.json\"\n"
     "    \"${MFD_ROOT_DIR}/assets/pages/mfd_tutorial_page1.json\"\n"
     "    \"${MFD_ROOT_DIR}/assets/pages/mfd_tutorial_page2.json\")\n"
     "\n"
     "if(MFD_CLIENT_TUTORIAL_MISSING_FILES)\n"
     "    message(STATUS\n"
     "        \"Skipping client_tutorial because the tutorial asset set is incomplete.\\n\"\n"
     "        \"Finish the editor tutorial and save the authored assets before reconfiguring.\")\n"
     "    return()\n"
     "endif()\n"
     "\n"
     "client_api_generate_ui(\n"
     "    WINDOW_JSON \"${MFD_CLIENT_TUTORIAL_WINDOW_JSON}\"\n"
     "    OUTPUT_HEADER \"${MFD_CLIENT_TUTORIAL_GENERATED_HEADER}\"\n"
     "    OUTPUT_SOURCE \"${MFD_CLIENT_TUTORIAL_GENERATED_SOURCE}\"\n"
     "    OUTPUT_MAP \"${MFD_ROOT_DIR}/assets/windows/mfd_tutorial.generated.map\"\n"
     "    NAMESPACE \"tutorial_ui\"\n"
     "    UI_CLASS_NAME \"TutorialUi\")",
     "The repository default build stays unchanged until the tutorial completes, and the target still self-skips whenever the authored asset set is incomplete.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review tutorial target registration",
     "The examples subtree only needs one explicit registration for the tutorial client, and the tutorial now writes it automatically at completion.",
     "",
     "examples/CMakeLists.txt",
     "add_subdirectory(mfd_framebuffer_stdout_plugin)\n"
     "add_subdirectory(mfd_editor_automation_sample_plugin)\n"
     "add_subdirectory(client_mockup)\n"
     "add_subdirectory(client_mockup_minimal)\n",
     "add_subdirectory(mfd_framebuffer_stdout_plugin)\n"
     "add_subdirectory(mfd_editor_automation_sample_plugin)\n"
     "add_subdirectory(client_mockup)\n"
     "add_subdirectory(client_mockup_minimal)\n"
     "add_subdirectory(client_tutorial)\n",
     "Finishing the integrated tutorial writes this registration into `examples/CMakeLists.txt`, so the generated walkthrough becomes available after the next CMake reconfigure.",
     "Next",
     1,
     1},
    {TutorialStepKind::FileReview,
     "Review the documentation path",
     "The integrated tutorial should leave the user with a clear next reading path across editor usage, generated API usage, and deeper contracts.",
     "",
     "docs/README.md",
     "| use `mfd_editor` | [Quick Start](./QUICKSTART.md) | [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md) |\n"
     "\n"
     "### Editor-Centric User\n"
     "1. [Quick Start](./QUICKSTART.md)\n"
     "2. [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md)\n"
     "3. [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)",
     "| use `mfd_editor` | [Quick Start](./QUICKSTART.md) | [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md), [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md), [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md) |\n"
     "\n"
     "### Editor-Centric User\n"
     "1. [Quick Start](./QUICKSTART.md)\n"
     "2. [Create A Window From Scratch In `mfd_editor`](./tutorials/13_create_window_from_editor.md)\n"
     "3. [Test A Window With The Mockup](./tutorials/03_test_with_mfd_mockup.md)\n"
     "4. [Use The Mockup As A Client API Reference](./tutorials/11_use_the_mockup_as_a_client_api_reference.md)\n"
     "5. [Generated Client API Standardization](./standards/mfd_generated_client_api_standardization.md)\n"
     "6. [Generated Client API Architecture](./architecture/generated_client_api.md)",
     "The tutorial now points directly to the next layers of the project: visual authoring, runtime validation with the mockup, generated C++17 API usage, and the architecture notes that explain the generated wrappers.",
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
