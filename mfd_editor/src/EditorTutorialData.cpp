/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "EditorTutorialData.h"

/**
 * @file
 * @brief Tutorial step definitions and helper rendering for editor coaching hints.
 */

#include <array>
#include <string>

#include <imgui.h>

namespace editor::tutorial
{
namespace
{
struct TutorialFileEditHint
{
    const char* filePath;
    const char* changeSummary;
    const char* snippet;
};

constexpr std::array<TutorialStepDefinition, 20> kTutorialSteps {{
    {"Create tutorial window", "Use File/New window from scratch, then create window mfd_tutorial (480x480, black bg, UDP 127.0.0.1:49000).", "menu_file_new_window"},
    {"Create radar-track reticle", "Create a library reticle template based on a primitive and name it tutorial_radar_track.", "popup_reticle_primitive"},
    {"Create circular target reticle", "Create a simple circular target reticle that will be layered on Page1.", "popup_reticle_primitive"},
    {"Create Page1", "Create Page1 and set layer 1 background to blue.", "menu_page_new"},
    {"Overlay full-page circular reticle", "Add the circular reticle on a second layer above the blue background.", "menu_page_new"},
    {"Enable outer clipping", "Right-click the circular reticle in preview and choose Clip outside.", "context_clip_outer"},
    {"Add text reticle layer", "Add a third layer with a text reticle and then hide this layer from the layer inspector.", "inspector_layers"},
    {"Add Page2", "Create a second page named Page2 with a simple title.", "menu_page_new"},
    {"Add strobe", "Configure a page strobe and explain that it will jump to each newly created track.", "coach_ok"},
    {"Save tutorial assets", "Save the tutorial assets from the File menu so window/page/reticle edits are persisted to disk.", "menu_file_save"},
    {"Change active page in code", "Show how the client switches between Page1 and Page2 (timer-based ActivatePage).", "code_page_switch"},
    {"Add dynamic reticle in code", "Show how to add/update a dynamic reticle from a template with UpsertDynamicReticle.", "code_dynamic_add"},
    {"Remove dynamic reticle in code", "Show how to remove the oldest dynamic reticle with RemoveDynamicReticle.", "code_dynamic_remove"},
    {"Modify static reticle attrs", "Show how to modify a static reticle (color/position/visibility) with SetReticle* commands.", "code_static_attrs"},
    {"Modify dynamic reticle", "Show how to update an existing dynamic reticle by re-upserting it with a patch.", "code_dynamic_modify"},
    {"Declutter dynamic reticles", "Show dynamic declutter by toggling visibility of one template set via SetDynamicReticleSetVisible.", "code_declutter"},
    {"Use generated API in client", "Add a client_tutorial step showing how to include and use generated UI API types/functions.", "code_generated_api"},
    {"Strobe feedback in client", "Show where the client receives strobe feedback packets (client_tutorial main loop).", "code_feedback"},
    {"RGBA32 pixel buffer", "Show where RGBA32 pixel buffer callback is read (mfd_tutorial main).", "code_pixels"},
    {"Enable tutorial targets", "Add tutorial targets in root CMakeLists once generated code exists.", "code_cmake"},
}};
} // namespace

std::span<const TutorialStepDefinition> Steps() noexcept
{
    return kTutorialSteps;
}

int StepCount() noexcept
{
    return static_cast<int>(kTutorialSteps.size());
}

void DrawTutorialFileHints(const int stepIndex)
{
    const auto drawEdit = [](const TutorialFileEditHint& hint)
    {
        ImGui::BulletText("%s", hint.filePath);
        ImGui::TextWrapped("Expected change: %s", hint.changeSummary);
        if (hint.snippet != nullptr && hint.snippet[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.10f, 0.14f, 1.0f));
            ImGui::BeginChild(
                std::string("##hint_").append(hint.filePath).c_str(),
                ImVec2(0.0f, 86.0f),
                true,
                ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(hint.snippet);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    };

    ImGui::SeparatorText("File modification view");
    switch (stepIndex)
    {
    case 0:
        drawEdit({"assets/windows/mfd_tutorial.json",
                  "Create a new window file (480x480, black background) and configure UDP command transport.",
                  "{ \"title\":\"MFD Tutorial\", \"size\":[480,480], \"commands\":{\"udp\":{\"address\":\"127.0.0.1\",\"port\":49000}} }"});
        break;
    case 1:
    case 2:
        drawEdit({"assets/reticles/mfd_tutorial_*.json",
                  "Create new reticle template JSON files from the editor reticle library.",
                  "{ \"id\":\"tutorial_radar_track\", \"elements\":[ ... ] }"});
        break;
    case 3:
    case 4:
    case 7:
        drawEdit({"assets/pages/mfd_tutorial_page1.json / mfd_tutorial_page2.json",
                  "Create Page1/Page2 and wire layers/reticles through editor actions.",
                  "{ \"name\":\"Page1\", \"editor\":{\"layers\":[...]}, \"staticReticles\":[...] }"});
        break;
    case 5:
        drawEdit({"assets/reticles/mfd_tutorial_circle.json",
                  "Set reticle clipping mode to outer and choose the clipping primitive.",
                  "{ \"clipMode\":\"outer\", \"clipPrimitive\":\"outer_circle\" }"});
        break;
    case 6:
        drawEdit({"assets/pages/mfd_tutorial_page1.json",
                  "Add a text layer and keep it hidden in the editor layer manager.",
                  "\"layers\": [{\"id\":\"text\", \"visible\":false}]"});
        break;
    case 8:
        drawEdit({"assets/pages/mfd_tutorial_page1.json",
                  "Add strobe configuration so runtime can move it across tracks.",
                  "\"strobe\": {\"id\":\"tutorial_strobe\", \"capture\": {\"shape\":\"circle\"}}"});
        break;
    case 9:
        drawEdit({"assets/windows/mfd_tutorial.json + assets/pages/*.json + assets/reticles/*.json",
                  "Use File > Save to persist all editor-side tutorial assets.",
                  "File -> Save (Ctrl+S)"});
        break;
    case 10:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Switch pages in code with ActivatePage and a timer.",
                  "activePage = (activePage == kPage1) ? std::string(kPage2) : std::string(kPage1);\nclient.ActivatePage(activePage);"});
        break;
    case 11:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Add/update one dynamic reticle with a patch.",
                  "client.UpsertDynamicReticle(kPage1, trackId, kTrackTemplate, patch);"});
        break;
    case 12:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Remove the oldest dynamic reticle id.",
                  "client.RemoveDynamicReticle(kPage1, trackIds.front());"});
        break;
    case 13:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Modify static reticle attributes.",
                  "client.SetReticleColor(kPage1, \"tutorial_circle_full\", {0,255,0,255});\nclient.SetReticlePosition(kPage1, \"tutorial_circle_full\", {0.0f,0.0f});\nclient.SetReticleVisible(kPage1, \"tutorial_circle_full\", true);"});
        break;
    case 14:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Modify an existing dynamic reticle by re-upserting same id with a new patch.",
                  "client.UpsertDynamicReticle(kPage1, existingId, kTrackTemplate, updatedPatch);"});
        break;
    case 15:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Declutter all dynamics of one template by toggling template-set visibility.",
                  "client.SetDynamicReticleSetVisible(kPage1, kTrackTemplate, false);\nclient.SetDynamicReticleSetVisible(kPage1, kTrackTemplate, true);"});
        break;
    case 16:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Include generated API header and call generated helpers/types when available.",
                  "#if __has_include(\"TutorialUi.h\")\n#include \"TutorialUi.h\"\n#endif"});
        break;
    case 17:
        drawEdit({"examples/client_tutorial/src/main.cpp",
                  "Poll feedback transport and decode strobe feedback packets.",
                  "const auto feedback = mfd::DeserializeStrobeStatusFeedback(raw, &error);"});
        break;
    case 18:
        drawEdit({"examples/mfd_tutorial/src/main.cpp",
                  "Read RGBA32 framebuffer callback in window launcher.",
                  "[](int width, int height, std::span<const std::byte> pixels) { ... }"});
        break;
    case 19:
        drawEdit({"CMakeLists.txt",
                  "Training step only: add tutorial targets once generated code exists.",
                  "add_subdirectory(examples/mfd_tutorial)\nadd_subdirectory(examples/client_tutorial)"});
        break;
    default:
        break;
    }
}
} // namespace editor::tutorial
