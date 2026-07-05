/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the DX11 dynamic-texture uploader.
 */

#include "Dx11TextureUploader.h"

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#endif

namespace lhld
{
Dx11TextureUploader::Dx11TextureUploader() = default;

Dx11TextureUploader::~Dx11TextureUploader()
{
    Release();
}

void Dx11TextureUploader::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) noexcept
{
    device_ = device;
    context_ = context;
}

bool Dx11TextureUploader::EnsureTexture([[maybe_unused]] const int width, [[maybe_unused]] const int height)
{
#if defined(_WIN32)
    if (device_ == nullptr || width <= 0 || height <= 0)
    {
        return false;
    }

    if (texture_ != nullptr && shaderResourceView_ != nullptr && width_ == width && height_ == height)
    {
        return true;
    }

    Release();

    D3D11_TEXTURE2D_DESC description {};
    description.Width = static_cast<UINT>(width);
    description.Height = static_cast<UINT>(height);
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device_->CreateTexture2D(&description, nullptr, &texture_)) || texture_ == nullptr)
    {
        texture_ = nullptr;
        return false;
    }

    if (FAILED(device_->CreateShaderResourceView(texture_, nullptr, &shaderResourceView_)) ||
        shaderResourceView_ == nullptr)
    {
        Release();
        return false;
    }

    width_ = width;
    height_ = height;
    return true;
#else
    return false;
#endif
}

bool Dx11TextureUploader::Upload([[maybe_unused]] const std::uint8_t* pixels,
                                 [[maybe_unused]] const int width,
                                 [[maybe_unused]] const int height,
                                 [[maybe_unused]] const std::size_t rowStrideBytes)
{
#if defined(_WIN32)
    // Reject a stride that cannot hold one full RGBA8 row so the per-row copy
    // below can never read past the source buffer.
    if (pixels == nullptr || context_ == nullptr || width <= 0 || height <= 0 ||
        rowStrideBytes < static_cast<std::size_t>(width) * 4U || !EnsureTexture(width, height))
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (FAILED(context_->Map(texture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return false;
    }

    const std::size_t sourceStride = rowStrideBytes;
    const std::size_t destinationStride = mapped.RowPitch;
    const std::size_t rowBytes =
        std::min(sourceStride, static_cast<std::size_t>(width) * 4U);
    auto* destination = static_cast<std::uint8_t*>(mapped.pData);
    for (int row = 0; row < height; ++row)
    {
        std::memcpy(
            destination + static_cast<std::size_t>(row) * destinationStride,
            pixels + static_cast<std::size_t>(row) * sourceStride,
            rowBytes);
    }
    context_->Unmap(texture_, 0);
    return true;
#else
    return false;
#endif
}

void* Dx11TextureUploader::TextureId() const noexcept
{
    return shaderResourceView_;
}

int Dx11TextureUploader::Width() const noexcept
{
    return width_;
}

int Dx11TextureUploader::Height() const noexcept
{
    return height_;
}

void Dx11TextureUploader::Release() noexcept
{
#if defined(_WIN32)
    if (shaderResourceView_ != nullptr)
    {
        shaderResourceView_->Release();
        shaderResourceView_ = nullptr;
    }
    if (texture_ != nullptr)
    {
        texture_->Release();
        texture_ = nullptr;
    }
#endif
    width_ = 0;
    height_ = 0;
}
} // namespace lhld
