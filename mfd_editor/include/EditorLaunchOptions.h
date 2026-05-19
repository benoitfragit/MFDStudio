/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Command-line option parsing for the editor executable.
 */

#include <filesystem>
#include <string>

namespace editor
{
/**
 * @brief Parsed launch options used to configure the editor shell.
 */
struct EditorLaunchOptions
{
    /** @brief Shows usage text and exits without starting the UI. */
    bool showHelp = false;
    /** @brief Optional authored asset root used for default editor paths. */
    std::filesystem::path assetDirectory {};
};

/**
 * @brief Result of command-line parsing.
 */
struct EditorLaunchOptionsParseResult
{
    /** @brief Parsed options when `error` is empty. */
    EditorLaunchOptions options {};
    /** @brief Human-readable validation error, or empty on success. */
    std::string error {};
};

/**
 * @brief Parses editor command-line arguments.
 * @param argc Argument count passed to `main`.
 * @param argv Argument array passed to `main`.
 * @return Parsed options and an optional error message.
 */
[[nodiscard]] EditorLaunchOptionsParseResult ParseEditorLaunchOptions(int argc, char* const argv[]);
} // namespace editor
