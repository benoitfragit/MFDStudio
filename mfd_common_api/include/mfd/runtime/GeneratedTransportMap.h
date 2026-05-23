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
/**
 * @brief Runtime-scoped identifier of one dynamic reticle instance.
 *
 * @note This identifier is distinct from `TransportId`:
 * `TransportId` addresses authored fixed objects emitted by the generator,
 * while `RuntimeDynamicId` addresses one runtime-created dynamic instance.
 */
using RuntimeDynamicId = std::uint64_t;

/** @brief Ownership scope of one primitive entry inside the generated transport map. */
enum class TransportPrimitiveOwnerKind
{
    Reticle,
    Template
};

/** @brief Top-level window metadata embedded in one generated transport map. */
struct TransportMapWindowInfo
{
    /** @brief Stable logical window identifier derived from the window JSON stem. */
    std::string name;
    /** @brief Authored window title kept for diagnostics and publication metadata. */
    std::string title;
    /** @brief Source window filename associated with this generated map. */
    std::string source;
};

/** @brief One page entry loaded from the generated transport map. */
struct TransportMapPageEntry
{
    /** @brief Stable generated transport id of the page. */
    TransportId id = 0;
    /** @brief Authored user-facing page name. */
    std::string name;
    /** @brief Normalized page name used by runtime lookups. */
    std::string normalizedName;
    /** @brief Indicates whether the authored page exposes at least one strobe entry. */
    bool hasStrobe = false;
    /** @brief Indicates whether this page is the authored default page. */
    bool defaultPage = false;
};

/** @brief One static reticle entry loaded from the generated transport map. */
struct TransportMapReticleEntry
{
    /** @brief Stable generated transport id of the static reticle. */
    TransportId id = 0;
    /** @brief Generated transport id of the owning page. */
    TransportId pageId = 0;
    /** @brief Authored reticle id exposed publicly on that page. */
    std::string reticleId;
    /** @brief Normalized reticle id used by runtime lookups. */
    std::string normalizedReticleId;
    /** @brief Human-readable source marker, currently `static`. */
    std::string source;
};

/** @brief One exposed primitive entry loaded from the generated transport map. */
struct TransportMapPrimitiveEntry
{
    /** @brief Stable generated transport id of the primitive. */
    TransportId id = 0;
    /** @brief Owning authored object kind. */
    TransportPrimitiveOwnerKind ownerKind = TransportPrimitiveOwnerKind::Reticle;
    /** @brief Generated transport id of the owning reticle or template. */
    TransportId ownerId = 0;
    /** @brief Authored primitive id exposed publicly. */
    std::string primitiveId;
    /** @brief Normalized primitive id used by runtime lookups. */
    std::string normalizedPrimitiveId;
    /** @brief Authored primitive type name kept for diagnostics and generation sanity checks. */
    std::string primitiveType;
    /** @brief Indicates whether the primitive is intentionally exposed to clients. */
    bool exposed = true;
};

/** @brief One dynamic-template entry loaded from the generated transport map. */
struct TransportMapTemplateEntry
{
    /** @brief Stable generated transport id of the dynamic reticle template. */
    TransportId id = 0;
    /** @brief Authored template id exposed publicly. */
    std::string templateId;
    /** @brief Normalized template id used by runtime lookups. */
    std::string normalizedTemplateId;
};

/** @brief One blink-type entry loaded from the generated transport map. */
struct TransportMapBlinkTypeEntry
{
    /** @brief Stable generated transport id of the blink type. */
    TransportId id = 0;
    /** @brief Generated transport id of the owning page. */
    TransportId pageId = 0;
    /** @brief Authored blink-type name exposed publicly. */
    std::string blinkType;
    /** @brief Normalized blink-type name used by runtime lookups. */
    std::string normalizedBlinkType;
    /** @brief Effective blink period in milliseconds. */
    std::uint32_t durationMs = 0;
};

/** @brief One strobe-catalog entry loaded from the generated transport map. */
struct TransportMapStrobeEntry
{
    /** @brief Stable generated transport id of the page-local strobe entry. */
    TransportId id = 0;
    /** @brief Generated transport id of the owning page. */
    TransportId pageId = 0;
    /** @brief Authored user-facing strobe name exposed by generated page wrappers. */
    std::string strobeName;
    /** @brief Normalized strobe name used by runtime lookups. */
    std::string normalizedStrobeName;
    /** @brief Public reticle id of the cursor reticle used by this strobe entry. */
    std::string reticleId;
    /** @brief Indicates whether this entry is the authored startup selection for its page. */
    bool defaultActive = false;
};

/** @brief Parsed content of one `*.generated.map` companion file. */
struct GeneratedTransportMap
{
    /** @brief Source map file loaded from disk. */
    std::filesystem::path sourceFile;
    /** @brief Schema version declared by the file. */
    int schemaVersion = 0;
    /** @brief Canonical compatibility hash shared with generated client code. */
    std::string mappingHash;
    /** @brief Top-level authored window metadata. */
    TransportMapWindowInfo window;
    /** @brief Generated page entries. */
    std::vector<TransportMapPageEntry> pages;
    /** @brief Generated static-reticle entries. */
    std::vector<TransportMapReticleEntry> reticles;
    /** @brief Generated exposed-primitive entries. */
    std::vector<TransportMapPrimitiveEntry> primitives;
    /** @brief Generated dynamic-template entries. */
    std::vector<TransportMapTemplateEntry> templates;
    /** @brief Generated page-local blink-type entries. */
    std::vector<TransportMapBlinkTypeEntry> blinkTypes;
    /** @brief Generated page-local strobe entries. */
    std::vector<TransportMapStrobeEntry> strobes;
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
