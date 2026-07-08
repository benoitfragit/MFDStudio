/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Minimal publishing facade of the reusable `hud_runtime` shared library.
 */

#include <memory>
#include <string>

#include "hud/HudRuntimeExport.h"
#include "hud/HudTypes.h"

namespace hud
{
/**
 * @brief Optional configuration consumed by `HudRuntimeClient::Initialize`.
 * @ingroup hud_integration
 */
struct HudRuntimeConfig
{
    /**
     * @brief Directory containing the HUD runtime assets.
     *
     * The directory must expose the staged HUD layout: `windows/hud_window.json`
     * (with its generated transport map next to it), `pages/`, `reticles/` and
     * `fonts/`. Leave empty to use the default staged layout lookup.
     */
    std::string assetRoot {};
};

/**
 * @brief Publishes HUD frames built from `HudInputSample` to the MFD runtime.
 * @ingroup hud_integration
 *
 * This class is the only entry point an external aircraft simulation needs: it
 * loads and validates the HUD window configuration, owns the startup and
 * realtime transports, converts each semantic input sample into generated UI
 * commands and publishes one command batch per `Publish()` call.
 *
 * The client is passive: it never steps a simulation, owns no pilot controls,
 * imposes no thread and no publication rate. The caller's main loop decides
 * when a frame is published.
 *
 * @note All transport, JSON-loading and generated-UI details stay inside the
 * implementation (PIMPL); no MFD or generated type crosses this header.
 */
class HUD_RUNTIME_API HudRuntimeClient
{
public:
    HudRuntimeClient();
    ~HudRuntimeClient();

    HudRuntimeClient(const HudRuntimeClient&) = delete;
    HudRuntimeClient& operator=(const HudRuntimeClient&) = delete;

    HudRuntimeClient(HudRuntimeClient&&) noexcept;
    HudRuntimeClient& operator=(HudRuntimeClient&&) noexcept;

    /**
     * @brief Initializes the HUD client from the default staged asset layout.
     * @param error Human-readable failure reason populated on failure.
     * @return `true` when HUD frames can be published.
     * @note Looks for `assets/windows/hud_window.json` relative to the working
     * directory first, then falls back to the in-repository asset path baked
     * into the generated UI. Calling `Initialize` on an already initialized
     * client rebuilds the transports and resends the generated startup state.
     */
    bool Initialize(std::string& error);

    /**
     * @brief Initializes the HUD client from an explicit asset directory.
     * @param config Runtime configuration; see `HudRuntimeConfig::assetRoot`.
     * @param error Human-readable failure reason populated on failure.
     * @return `true` when HUD frames can be published.
     */
    bool Initialize(const HudRuntimeConfig& config, std::string& error);

    /**
     * @brief Converts one semantic input sample to HUD commands and publishes it.
     * @param input Complete SI-unit aircraft/target/weapon snapshot for one frame.
     * @param error Human-readable failure reason populated on failure.
     * @return `true` when the frame was handed to the realtime transport.
     * @pre `Initialize()` returned `true` and no publishing error occurred since.
     * @note A `false` return reports a lost transport or a closed HUD window;
     * call `Initialize()` again to reconnect.
     */
    bool Publish(const HudInputSample& input, std::string& error);

    /**
     * @brief Publishes a shutdown caption, flushes and releases the transports.
     * @note Safe to call multiple times; also invoked by the destructor.
     */
    void Shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace hud
