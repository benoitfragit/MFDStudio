/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Runtime companion mapping types loaded from `*.generated.map` files.
 */

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "mfd/MfdExport.h"

namespace mfd
{
/** @brief Stable transport identifier emitted by the generator. */
using TransportId = std::uint64_t;

/** @brief Ownership scope of one primitive entry inside the generated transport map. */
enum class TransportPrimitiveOwnerKind
{
    Reticle,
    Template
};

/** @brief Top-level window metadata embedded in one generated transport map. */
struct TransportMapWindowInfo
{
    std::string name;
    std::string title;
    std::string source;
};

/** @brief One page entry loaded from the generated transport map. */
struct TransportMapPageEntry
{
    TransportId id = 0;
    std::string name;
    std::string normalizedName;
    bool hasStrobe = false;
    bool defaultPage = false;
};

/** @brief One static reticle entry loaded from the generated transport map. */
struct TransportMapReticleEntry
{
    TransportId id = 0;
    TransportId pageId = 0;
    std::string reticleId;
    std::string normalizedReticleId;
    std::string source;
};

/** @brief One exposed primitive entry loaded from the generated transport map. */
struct TransportMapPrimitiveEntry
{
    TransportId id = 0;
    TransportPrimitiveOwnerKind ownerKind = TransportPrimitiveOwnerKind::Reticle;
    TransportId ownerId = 0;
    std::string primitiveId;
    std::string normalizedPrimitiveId;
    std::string primitiveType;
    bool exposed = true;
};

/** @brief One dynamic-template entry loaded from the generated transport map. */
struct TransportMapTemplateEntry
{
    TransportId id = 0;
    std::string templateId;
    std::string normalizedTemplateId;
};

/** @brief One blink-type entry loaded from the generated transport map. */
struct TransportMapBlinkTypeEntry
{
    TransportId id = 0;
    TransportId pageId = 0;
    std::string blinkType;
    std::string normalizedBlinkType;
    std::uint32_t durationMs = 0;
};

/** @brief Parsed content of one `*.generated.map` companion file. */
struct GeneratedTransportMap
{
    std::filesystem::path sourceFile;
    int schemaVersion = 0;
    std::string mappingHash;
    TransportMapWindowInfo window;
    std::vector<TransportMapPageEntry> pages;
    std::vector<TransportMapReticleEntry> reticles;
    std::vector<TransportMapPrimitiveEntry> primitives;
    std::vector<TransportMapTemplateEntry> templates;
    std::vector<TransportMapBlinkTypeEntry> blinkTypes;
};

/**
 * @brief Resolves the conventional companion map path next to one window JSON.
 * @param windowFile Root window JSON file.
 * @return `<window>.generated.map` path next to the window JSON.
 */
MFD_API std::filesystem::path CompanionGeneratedTransportMapPath(const std::filesystem::path& windowFile);

/**
 * @brief Attempts to load the companion generated transport map next to one window JSON.
 * @param windowFile Root window JSON file.
 * @param error Optional output string receiving the validation or I/O failure.
 * @return Parsed map when the companion exists and is valid.
 *
 * @note Missing companion files are reported as `std::nullopt` without
 * populating `error`, which keeps legacy window assets loadable until they are
 * migrated to generated maps.
 */
MFD_API std::optional<GeneratedTransportMap> TryLoadGeneratedTransportMap(const std::filesystem::path& windowFile,
                                                                          std::string* error = nullptr);
} // namespace mfd
