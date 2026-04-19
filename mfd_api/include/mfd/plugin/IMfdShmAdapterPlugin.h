/*
 * This file is part of MFDStudio.
 */
#pragma once

/**
 * @file
 * @brief Interface for loadable MFD SHM adapter plugins converting business payloads to UserCommand.
 */

#include <string>
#include <vector>

#include "mfd/MfdExport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/CommandTransport.h"

namespace mfd
{
/** @brief Runtime plugin interface converting SHM business payloads into generic user commands. */
class MFD_API IMfdShmAdapterPlugin
{
public:
    virtual ~IMfdShmAdapterPlugin() = default;

    /** @brief Initializes the plugin with command SHM transport settings. */
    virtual bool Initialize(const WindowShmCommandTransport& config) = 0;
    /** @brief Polls SHM and appends translated generic commands. */
    virtual bool Poll(std::vector<UserCommand>& outCommands) = 0;
    /** @brief Returns the last plugin-side error string. */
    virtual std::string LastError() const = 0;
};

/** @brief Factory signature used to construct an SHM adapter plugin from a shared library. */
using MfdShmAdapterFactory = IMfdShmAdapterPlugin* (*)();
} // namespace mfd
