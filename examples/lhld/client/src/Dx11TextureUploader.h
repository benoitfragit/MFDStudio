/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Uploads CPU RGBA8 frames into a dynamic DX11 shader-resource texture.
 *
 * @note This unit includes Direct3D headers and never includes raylib, so the
 * raylib/`windows.h` symbol clash cannot occur.
 */

#include <cstddef>
#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace lhld
{
/**
 * @brief Owns one DX11 dynamic texture used to present the offscreen MFD image.
 */
class Dx11TextureUploader
{
public:
    Dx11TextureUploader();
    ~Dx11TextureUploader();

    Dx11TextureUploader(const Dx11TextureUploader&) = delete;
    Dx11TextureUploader& operator=(const Dx11TextureUploader&) = delete;

    /**
     * @brief Stores the DX11 device and context used to allocate the texture.
     * @param device DX11 device owned by the host.
     * @param context DX11 immediate context owned by the host.
     */
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context) noexcept;

    /**
     * @brief Uploads one RGBA8 frame, resizing the texture when needed.
     * @param pixels Borrowed RGBA8 buffer, row-major top-left.
     * @param width Frame width in pixels.
     * @param height Frame height in pixels.
     * @param rowStrideBytes Source row stride in bytes.
     * @return `true` when the current texture holds the uploaded frame.
     */
    bool Upload(const std::uint8_t* pixels, int width, int height, std::size_t rowStrideBytes);

    /**
     * @brief Returns the shader-resource view as an opaque handle for ImGui.
     * @return `ID3D11ShaderResourceView*` as `void*`, or nullptr when empty.
     */
    void* TextureId() const noexcept;

    /** @brief Returns the current texture width in pixels. */
    int Width() const noexcept;
    /** @brief Returns the current texture height in pixels. */
    int Height() const noexcept;

    /** @brief Releases the texture and its shader-resource view. */
    void Release() noexcept;

private:
    /** @brief Ensures the texture matches the requested size. */
    bool EnsureTexture(int width, int height);

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* shaderResourceView_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};
} // namespace lhld
