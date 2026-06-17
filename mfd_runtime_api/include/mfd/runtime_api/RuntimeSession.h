/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Public reusable runtime session API kept independent from `mfd_api` headers.
 */

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/runtime_api/RuntimeApiExport.h"

namespace mfd::runtime_api
{
namespace internal
{
struct RuntimeSessionInternalAccess;
}

/**
 * @brief Lightweight public description of one authored window.
 */
struct RuntimeWindowInfo
{
    /** @brief Host window title authored in the root window JSON. */
    std::string title;
    /** @brief Authored window width in pixels. */
    int width = 0;
    /** @brief Authored window height in pixels. */
    int height = 0;
    /** @brief Requested host refresh rate, or `0` when uncapped. */
    int targetFps = 0;
};

/**
 * @brief Lightweight public description of one runtime page.
 */
struct RuntimePageInfo
{
    /** @brief User-facing page name. */
    std::string name;
    /** @brief Optional page title. */
    std::string title;
    /** @brief Indicates whether this page is currently active. */
    bool active = false;
};

/**
 * @brief Reusable host runtime for one authored window without exposing `mfd_api`.
 */
class MFD_RUNTIME_API RuntimeSession
{
public:
    RuntimeSession();
    ~RuntimeSession();

    RuntimeSession(const RuntimeSession&) = delete;
    RuntimeSession& operator=(const RuntimeSession&) = delete;
    RuntimeSession(RuntimeSession&&) = delete;
    RuntimeSession& operator=(RuntimeSession&&) = delete;

    /**
     * @brief Loads a root window JSON and starts the associated runtime services.
     * @param windowFile Path to the root window JSON file.
     * @param error Output string receiving the failure description on error.
     * @return `true` on success, otherwise `false`.
     */
    bool LoadWindowFile(const std::filesystem::path& windowFile, std::string& error);

    /**
     * @brief Reloads the last successfully loaded root window JSON file.
     * @param error Output string receiving the failure description on error.
     * @return `true` on success, otherwise `false`.
     */
    bool Reload(std::string& error);

    /**
     * @brief Advances command application and runtime feedback publication.
     * @param deltaSeconds Elapsed frame time in seconds.
     */
    void Advance(float deltaSeconds);

    /**
     * @brief Emits one final window lifecycle `Closing` feedback payload.
     *
     * @note Call this once before tearing the session down so connected clients
     * can immediately detect a graceful shutdown and reset their transports.
     * The call is idempotent and a no-op when no feedback transport is
     * configured. Abrupt termination (crash or kill) is still detectable on the
     * client side through the absence of the lifecycle heartbeat.
     */
    void NotifyClosing();

    /**
     * @brief Returns the loaded root window JSON file path.
     * @return Loaded root window JSON file path, or an empty path before the first successful load.
     */
    [[nodiscard]] const std::filesystem::path& WindowFile() const noexcept;

    /**
     * @brief Returns the public authored window metadata.
     * @return Lightweight public window metadata snapshot.
     */
    [[nodiscard]] RuntimeWindowInfo WindowInfo() const;

    /**
     * @brief Returns lightweight information for every authored page.
     * @return Ordered page list.
     */
    [[nodiscard]] std::vector<RuntimePageInfo> Pages() const;

    /**
     * @brief Returns the active page name.
     * @return Current active page name, or an empty string before the first successful load.
     */
    [[nodiscard]] std::string ActivePageName() const;

    /**
     * @brief Activates one page by name.
     * @param pageName User-facing page name to activate.
     * @return `true` when the page exists and is now active, otherwise `false`.
     */
    [[nodiscard]] bool SetActivePage(std::string_view pageName);

    /**
     * @brief Returns the latest command-side runtime status string.
     * @return Human-readable command-side status.
     */
    [[nodiscard]] const std::string& LastCommandStatus() const noexcept;

    /**
     * @brief Returns the latest feedback-side runtime status string.
     * @return Human-readable feedback-side status.
     */
    [[nodiscard]] const std::string& LastFeedbackStatus() const noexcept;

    /**
     * @brief Indicates whether at least one client batch already reached the runtime.
     * @return `true` after the first drained inbound batch.
     */
    [[nodiscard]] bool ReceivedFirstClientCommand() const noexcept;

private:
    friend struct internal::RuntimeSessionInternalAccess;

    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd::runtime_api
