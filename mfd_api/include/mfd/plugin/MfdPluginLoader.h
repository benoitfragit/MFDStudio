/*
 * This file is part of MFDStudio.
 */
#pragma once

/**
 * @file
 * @brief Runtime loader owning a shared-library SHM adapter plugin instance.
 */

#include <memory>
#include <string>

#include "mfd/MfdExport.h"
#include "mfd/plugin/IMfdShmAdapterPlugin.h"

namespace mfd
{
class MFD_API MfdPluginLoader
{
public:
    MfdPluginLoader() = default;
    ~MfdPluginLoader();

    MfdPluginLoader(const MfdPluginLoader&) = delete;
    MfdPluginLoader& operator=(const MfdPluginLoader&) = delete;

    bool Load(const std::string& path, const std::string& symbol);
    IMfdShmAdapterPlugin* Plugin() const noexcept;
    std::string LastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_ {};
    std::string lastError_ {};
};
} // namespace mfd
