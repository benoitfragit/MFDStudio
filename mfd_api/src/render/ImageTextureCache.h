/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Lazy texture cache used by the renderer and the editor for image primitives.
 */

#include <filesystem>
#include <memory>

#include <raylib.h>

namespace mfd
{
/**
 * @brief Loads and reuses raylib textures keyed by authored image file path.
 */
class ImageTextureCache
{
public:
    ImageTextureCache();
    ~ImageTextureCache();

    ImageTextureCache(const ImageTextureCache&) = delete;
    ImageTextureCache& operator=(const ImageTextureCache&) = delete;
    ImageTextureCache(ImageTextureCache&&) noexcept;
    ImageTextureCache& operator=(ImageTextureCache&&) noexcept;

    /**
     * @brief Returns the cached texture for one image file, loading it on first use.
     * @param file Image file path resolved by the JSON loader.
     * @return Pointer to the ready texture, or `nullptr` when loading failed.
     */
    const Texture2D* Resolve(const std::filesystem::path& file);

    /**
     * @brief Releases every cached texture.
     */
    void Clear() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mfd
