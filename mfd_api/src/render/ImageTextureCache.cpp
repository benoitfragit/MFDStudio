/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for ImageTextureCache.
 */

#include "ImageTextureCache.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

namespace mfd
{
namespace
{
// A failed load is retried at most once per interval so a file that appears on disk after a first miss
// is eventually picked up, without re-hitting the filesystem every frame for a genuinely missing file.
constexpr std::chrono::seconds kImageRetryInterval {2};
constexpr std::size_t kMaximumCacheEntries = 256;
constexpr std::size_t kMaximumTextureBytes = 256U * 1024U * 1024U;

std::size_t EstimateTextureBytes(const Texture2D& texture) noexcept
{
    std::size_t byteCount = 0;
    int mipWidth = texture.width;
    int mipHeight = texture.height;
    const int mipCount = (texture.mipmaps > 0) ? texture.mipmaps : 1;

    for (int mipIndex = 0; mipIndex < mipCount; ++mipIndex)
    {
        const int mipBytes = GetPixelDataSize(mipWidth, mipHeight, texture.format);
        if (mipBytes <= 0 || byteCount > (std::numeric_limits<std::size_t>::max() -
                                           static_cast<std::size_t>(mipBytes)))
        {
            return std::numeric_limits<std::size_t>::max();
        }

        byteCount += static_cast<std::size_t>(mipBytes);
        mipWidth = (mipWidth > 1) ? (mipWidth / 2) : 1;
        mipHeight = (mipHeight > 1) ? (mipHeight / 2) : 1;
    }

    return byteCount;
}
} // namespace

struct ImageTextureCache::Impl
{
    struct Entry
    {
        Texture2D texture {};
        bool ready = false;
        bool attempted = false;
        std::chrono::steady_clock::time_point lastAttempt {};
        std::size_t textureBytes = 0;
        std::uint64_t lastAccess = 0;
    };

    std::unordered_map<std::string, Entry> entries {};
    std::size_t textureBytes = 0;
    std::uint64_t accessSequence = 0;

    void Touch(Entry& entry) noexcept
    {
        ++accessSequence;
        entry.lastAccess = accessSequence;
    }

    void Release(Entry& entry, const bool windowReady) noexcept
    {
        if (entry.ready && windowReady)
        {
            UnloadTexture(entry.texture);
        }

        textureBytes = (entry.textureBytes <= textureBytes) ? (textureBytes - entry.textureBytes) : 0;
        entry = {};
    }

    bool RemoveLeastRecentlyUsed(const std::string& protectedKey) noexcept
    {
        auto victim = entries.end();
        for (auto current = entries.begin(); current != entries.end(); ++current)
        {
            if (current->first == protectedKey)
            {
                continue;
            }

            if (victim == entries.end() || current->second.lastAccess < victim->second.lastAccess)
            {
                victim = current;
            }
        }

        if (victim == entries.end())
        {
            return false;
        }

        Release(victim->second, IsWindowReady());
        entries.erase(victim);
        return true;
    }

    void TrimToBudget(const std::string& protectedKey) noexcept
    {
        while (entries.size() > kMaximumCacheEntries || textureBytes > kMaximumTextureBytes)
        {
            if (!RemoveLeastRecentlyUsed(protectedKey))
            {
                break;
            }
        }
    }
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
    auto entryPosition = impl_->entries.find(key);
    if (entryPosition == impl_->entries.end())
    {
        entryPosition = impl_->entries.emplace(key, Impl::Entry {}).first;
    }

    Impl::Entry& entry = entryPosition->second;
    impl_->Touch(entry);
    if (entry.ready)
    {
        return &entry.texture;
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (entry.attempted && (now - entry.lastAttempt) < kImageRetryInterval)
    {
        return nullptr;
    }

    entry.attempted = true;
    entry.lastAttempt = now;
    entry.texture = LoadTexture(normalizedPath.string().c_str());
    if (entry.texture.id == 0)
    {
        entry.texture = {};
        impl_->TrimToBudget(key);
        return nullptr;
    }

    SetTextureFilter(entry.texture, TEXTURE_FILTER_BILINEAR);
    entry.ready = true;
    entry.textureBytes = EstimateTextureBytes(entry.texture);
    if (entry.textureBytes > kMaximumTextureBytes)
    {
        impl_->Release(entry, IsWindowReady());
        impl_->entries.erase(entryPosition);
        return nullptr;
    }

    impl_->textureBytes += entry.textureBytes;
    impl_->TrimToBudget(key);
    return &entry.texture;
}

void ImageTextureCache::Clear() noexcept
{
    if (impl_ == nullptr)
    {
        return;
    }

    const bool windowReady = IsWindowReady();
    for (auto& entry : impl_->entries)
    {
        impl_->Release(entry.second, windowReady);
    }

    impl_->entries.clear();
    impl_->textureBytes = 0;
}
} // namespace mfd
