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
#include <algorithm>
#include <cmath>
#include <vector>
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
struct ScopedImage
{
    Image image {};
    ~ScopedImage() { if (image.data) UnloadImage(image); }
};
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
        bool alphaAttempted = false;
        int alphaWidth = 0, alphaHeight = 0;
        std::vector<unsigned char> alpha;
    };

    std::unordered_map<std::string, Entry> entries {};
    std::size_t textureBytes = 0;
    std::uint64_t accessSequence = 0;

    void CaptureAlpha(Entry& entry, const Image& image)
    {
        entry.alphaAttempted = true;
        // Bound decode sampling and mask memory independently from texture dimensions.
        if (!image.data || image.width <= 0 || image.height <= 0 || image.width > 4096 || image.height > 4096) return;
        const auto count = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
        if (entry.textureBytes > kMaximumTextureBytes - count) return;
        entry.alpha.resize(count);
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x)
                entry.alpha[static_cast<std::size_t>(y) * image.width + x] = GetImageColor(image, x, y).a;
        entry.alphaWidth = image.width; entry.alphaHeight = image.height;
        entry.textureBytes += count;
        textureBytes += count;
    }

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

ImageTextureCache& ImageTextureCache::operator=(ImageTextureCache&& other) noexcept
{
    if (this != &other)
    {
        Clear();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

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

void ImageTextureCache::Preload(const std::filesystem::path& file, const std::string_view encodedBytes)
{
    if (!impl_ || encodedBytes.empty() || encodedBytes.size() > 16U * 1024U * 1024U || !IsWindowReady()) return;
    const auto key = file.lexically_normal().generic_string();
    if (impl_->entries.count(key)) return;
    auto& entry = impl_->entries[key];
    entry.attempted = true;
    entry.lastAttempt = std::chrono::steady_clock::now();
    // raylib consumes unsigned encoded bytes; no ownership transfer or alignment requirement.
    ScopedImage loaded {LoadImageFromMemory(file.extension().string().c_str(),
        reinterpret_cast<const unsigned char*>(encodedBytes.data()), static_cast<int>(encodedBytes.size()))};
    const auto& image = loaded.image;
    if (image.data)
    {
        const auto bytes = GetPixelDataSize(image.width, image.height, image.format);
        if (bytes > 0 && static_cast<std::size_t>(bytes) <= kMaximumTextureBytes)
        {
            entry.texture = LoadTextureFromImage(image);
            if (entry.texture.id)
            {
                entry.ready = true;
                entry.textureBytes = EstimateTextureBytes(entry.texture);
                impl_->textureBytes += entry.textureBytes;
                impl_->CaptureAlpha(entry, image);
            }
        }
    }
    if (entry.texture.id)
    {
        entry.ready = true;

        SetTextureFilter(entry.texture, TEXTURE_FILTER_BILINEAR);
    }
    impl_->Touch(entry);
    impl_->TrimToBudget(key);
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
bool ImageTextureCache::ContainsOpaquePixel(const std::filesystem::path& file, const float u, const float v)
{
    if (!std::isfinite(u) || !std::isfinite(v) || u < 0 || u > 1 || v < 0 || v > 1 || !Resolve(file)) return false;
    const auto key = file.lexically_normal().generic_string();
    auto& entry = impl_->entries.at(key);
    if (!entry.alphaAttempted)
    {
        entry.alphaAttempted = true;
        std::error_code error;
        const auto bytes = std::filesystem::file_size(file, error);
        if (!error && bytes <= 16U * 1024U * 1024U)
        {
            ScopedImage loaded {LoadImage(file.string().c_str())};
            impl_->CaptureAlpha(entry, loaded.image);
            impl_->TrimToBudget(key);
        }
    }
    if (entry.alpha.empty()) return true;
    const float x = u * entry.alphaWidth - 0.5f, y = v * entry.alphaHeight - 0.5f;
    const int left = static_cast<int>(std::floor(x)), top = static_cast<int>(std::floor(y));
    const float dx = x - left, dy = y - top;
    float alpha = 0;
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 2; ++column)
        {
            const auto offset = static_cast<std::size_t>(std::clamp(top + row, 0, entry.alphaHeight - 1)) * entry.alphaWidth +
                std::clamp(left + column, 0, entry.alphaWidth - 1);
            alpha += entry.alpha[offset] * (column ? dx : 1 - dx) * (row ? dy : 1 - dy);
        }
    return alpha > 0;
}
} // namespace mfd
