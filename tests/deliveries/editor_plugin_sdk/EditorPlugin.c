/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/editor/AutomationPlugin.h"

#include <string.h>

static MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL StartPlugin(
    void* plugin_context,
    const MfdEditorAutomationHostApi* host,
    MfdEditorUtf8Buffer* error)
{
    (void)plugin_context;
    (void)host;
    if (error != NULL)
    {
        error->size = 0U;
        if (error->data != NULL && error->capacity > 0U)
        {
            error->data[0] = '\0';
        }
    }

    return MfdEditorAutomationResultCode_Success;
}

static MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL TickPlugin(
    void* plugin_context,
    MfdEditorUtf8Buffer* error)
{
    return StartPlugin(plugin_context, NULL, error);
}

static void MFD_EDITOR_AUTOMATION_CALL StopPlugin(void* plugin_context)
{
    (void)plugin_context;
}

static void MFD_EDITOR_AUTOMATION_CALL DestroyPlugin(void* plugin_context)
{
    (void)plugin_context;
}

MFD_EDITOR_AUTOMATION_EXPORT MfdEditorAutomationResultCode MFD_EDITOR_AUTOMATION_CALL MfdGetEditorAutomationPluginApi(
    MfdEditorAutomationPluginApi* out_api,
    MfdEditorUtf8Buffer* error)
{
    static const char plugin_id[] = "delivery.editor.sdk.smoke";
    static const char display_name[] = "Delivery Editor SDK Smoke";

    if (out_api == NULL)
    {
        if (error != NULL)
        {
            static const char message[] = "out_api is required";
            error->size = sizeof(message) - 1U;
            if (error->data != NULL && error->capacity > 0U)
            {
                const size_t writable = (error->capacity - 1U) < (sizeof(message) - 1U)
                    ? (error->capacity - 1U)
                    : (sizeof(message) - 1U);
                memcpy(error->data, message, writable);
                error->data[writable] = '\0';
            }
        }

        return MfdEditorAutomationResultCode_InvalidArgument;
    }

    memset(out_api, 0, sizeof(*out_api));
    out_api->struct_size = sizeof(*out_api);
    out_api->info.struct_size = sizeof(out_api->info);
    out_api->info.abi_version = MFD_EDITOR_AUTOMATION_PLUGIN_ABI_VERSION;
    out_api->info.plugin_id.data = plugin_id;
    out_api->info.plugin_id.size = sizeof(plugin_id) - 1U;
    out_api->info.display_name.data = display_name;
    out_api->info.display_name.size = sizeof(display_name) - 1U;
    out_api->start = &StartPlugin;
    out_api->tick = &TickPlugin;
    out_api->stop = &StopPlugin;
    out_api->destroy = &DestroyPlugin;

    if (error != NULL)
    {
        error->size = 0U;
        if (error->data != NULL && error->capacity > 0U)
        {
            error->data[0] = '\0';
        }
    }

    return MfdEditorAutomationResultCode_Success;
}
