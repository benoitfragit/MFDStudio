/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Test automation plugin DLL exposing one mismatched stable ABI version.
 */

#include "mfd/editor/AutomationPlugin.h"

#include <cstring>
#include <new>

namespace
{
MfdEditorStringView MakeView(const char* text) noexcept
{
    return MfdEditorStringView {text, text == nullptr ? 0U : std::strlen(text)};
}

struct BadAbiAutomationPluginContext
{
};

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(void*,
                                                                     const MfdEditorAutomationHostApi*,
                                                                     MfdEditorUtf8Buffer*) noexcept
{
    return MfdEditorAutomationResultCode_Success;
}

MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(void*, MfdEditorUtf8Buffer*) noexcept
{
    return MfdEditorAutomationResultCode_Success;
}

void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void*) noexcept
{
}

void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* pluginContext) noexcept
{
    delete static_cast<BadAbiAutomationPluginContext*>(pluginContext);
}
} // namespace

extern "C" MFD_EDITOR_AUTOMATION_EXPORT MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL
MfdGetEditorAutomationPluginApi(MfdEditorAutomationPluginApi* outApi, MfdEditorUtf8Buffer*) noexcept
{
    if (outApi == nullptr)
    {
        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    auto* context = new (std::nothrow) BadAbiAutomationPluginContext();
    if (context == nullptr)
    {
        return MfdEditorAutomationResultCode_InternalFailure;
    }

    *outApi = {};
    outApi->struct_size = sizeof(*outApi);
    outApi->info.struct_size = sizeof(outApi->info);
    outApi->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION + 1U;
    outApi->info.plugin_id = MakeView("test.bad.abi.automation.plugin");
    outApi->info.display_name = MakeView("MFD Editor Bad ABI Automation Plugin");
    outApi->plugin_context = context;
    outApi->start = &StartPlugin;
    outApi->tick = &TickPlugin;
    outApi->stop = &StopPlugin;
    outApi->destroy = &DestroyPlugin;
    return MfdEditorAutomationResultCode_Success;
}

