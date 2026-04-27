/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for ImageTextureCache.
 */

#include "mfd/render/ImageTextureCache.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace mfd
{
struct ImageTextureCache::Impl
{
    struct Entry
    {
        Texture2D texture {};
        bool ready = false;
        bool attempted = false;
    };

    std::unordered_map<std::string, Entry> entries {};
};

ImageTextureCache::ImageTextureCache()
    : impl_(std::make_unique<Impl>())
{
}

ImageTextureCache::~ImageTextureCache()
{
    Clear();
}

ImageTextureCache::ImageTextureCache(ImageTextureCache&&) noexcept = default;

ImageTextureCache& ImageTextureCache::operator=(ImageTextureCache&&) noexcept = default;

const Texture2D* ImageTextureCache::Resolve(const std::filesystem::path& file)
{
    if (impl_ == nullptr || file.empty())
    {
        return nullptr;
    }

    const std::filesystem::path normalizedPath = file.lexically_normal();
    const std::string key = normalizedPath.generic_string();
    Impl::Entry& entry = impl_->entries[key];
    if (entry.ready)
    {
        return &entry.texture;
    }

    if (entry.attempted)
    {
        return nullptr;
    }

    entry.attempted = true;
    entry.texture = LoadTexture(normalizedPath.string().c_str());
    if (entry.texture.id == 0)
    {
        entry.texture = {};
        return nullptr;
    }

    SetTextureFilter(entry.texture, TEXTURE_FILTER_BILINEAR);
    entry.ready = true;
    return &entry.texture;
}

void ImageTextureCache::Clear() noexcept
{
    if (impl_ == nullptr)
    {
        return;
    }

    for (auto& [path, entry] : impl_->entries)
    {
        (void)path;
        if (entry.ready)
        {
            UnloadTexture(entry.texture);
        }
    }

    impl_->entries.clear();
}
} // namespace mfd
