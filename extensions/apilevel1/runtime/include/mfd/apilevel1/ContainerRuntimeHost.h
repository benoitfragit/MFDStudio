/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/** @file @brief Host-side lifetime-safe loader for the LEVEL 1 runtime DLL. */

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "mfd/apilevel1/ContainerTypes.h"

namespace mfd::api1
{
/** @brief Loads, validates and invokes `mfd_containers_api1` through its C ABI. */
class ContainerRuntimeHost final
{
public:
    ContainerRuntimeHost();
    ~ContainerRuntimeHost();
    ContainerRuntimeHost(const ContainerRuntimeHost&) = delete;
    ContainerRuntimeHost& operator=(const ContainerRuntimeHost&) = delete;
    ContainerRuntimeHost(ContainerRuntimeHost&&) noexcept;
    ContainerRuntimeHost& operator=(ContainerRuntimeHost&&) noexcept;

    /**
     * @brief Loads a specific runtime extension DLL and performs the ABI handshake.
     * @param libraryFile DLL path.
     * @param error Receives a precise diagnostic.
     * @return `true` when all required capabilities are available.
     */
    [[nodiscard]] bool Load(const std::filesystem::path& libraryFile, std::string& error);
    /**
     * @brief Finds the extension beside the process or the authored Window.
     * @param windowFile Loaded root Window JSON path used as a secondary search location.
     * @param error Receives a precise diagnostic including searched locations.
     * @return `true` on a compatible handshake.
     */
    [[nodiscard]] bool LoadForProcess(const std::filesystem::path& windowFile, std::string& error);
    /** @brief Releases the function table before unloading the DLL. */
    void Unload() noexcept;
    /** @brief Returns whether a compatible extension is loaded. */
    [[nodiscard]] bool IsAvailable() const noexcept;
    /**
     * @brief Resolves layout through the loaded extension.
     * @param definitions Authored flat container catalog.
     * @param output Replaced with deterministic pre-order layout.
     * @param error Receives extension diagnostics.
     * @return `true` on success.
     */
    [[nodiscard]] bool Evaluate(const std::vector<ContainerDefinition>& definitions,
                                std::vector<ContainerLayoutNode>& output,
                                std::string& error) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd::api1
