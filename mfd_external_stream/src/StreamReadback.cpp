#include "StreamReadback.h"
#include <cstring>
#include <limits>
#include <stdexcept>
#if defined(_WIN32)
namespace mfd::stream::detail
{
namespace
{
constexpr GLenum kPixelUnpackBuffer = 0x88EC;
constexpr GLenum kPixelUnpackBufferBinding = 0x88EF;
/** @brief Isolates uploads from the renderer's texture, PBO and pixel-store state. */
class UploadState
{
public:
    explicit UploadState(const GlInterop& api) : api_(api)
    {
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
        glGetIntegerv(kPixelUnpackBufferBinding, &buffer_);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment_);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength_);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows_);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels_);
        api_.bindBuffer(kPixelUnpackBuffer, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }
    ~UploadState()
    {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
        api_.bindBuffer(kPixelUnpackBuffer, static_cast<GLuint>(buffer_));
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment_);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength_);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows_);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels_);
    }
    UploadState(const UploadState&) = delete;
    UploadState& operator=(const UploadState&) = delete;
private:
    const GlInterop& api_;
    GLint texture_ = 0, buffer_ = 0, alignment_ = 0, rowLength_ = 0, skipRows_ = 0, skipPixels_ = 0;
};
/** @brief Releases a successful staging map on every exit path. */
class StagingMap
{
public:
    StagingMap(ID3D11DeviceContext* context, ID3D11Texture2D* texture)
        : context_(context), texture_(texture)
    {
        const HRESULT result = context_->Map(texture_, 0, D3D11_MAP_READ,
                                             D3D11_MAP_FLAG_DO_NOT_WAIT, &data_);
        if (result == DXGI_ERROR_WAS_STILL_DRAWING) return;
        CheckHr(result, "Map external stream staging texture");
        mapped_ = true;
    }
    ~StagingMap() { if (mapped_) context_->Unmap(texture_, 0); }
    StagingMap(const StagingMap&) = delete;
    StagingMap& operator=(const StagingMap&) = delete;
    bool Mapped() const noexcept { return mapped_; }
    const D3D11_MAPPED_SUBRESOURCE& Data() const noexcept { return data_; }
private:
    ID3D11DeviceContext* context_;
    ID3D11Texture2D* texture_;
    D3D11_MAPPED_SUBRESOURCE data_ {};
    bool mapped_ = false;
};
void CheckGl(const char* operation)
{
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error(operation);
}
}
CpuReadback::CpuReadback(const GlInterop& api, ID3D11Device* device, const std::uint32_t width,
                         const std::uint32_t height, const std::uint32_t format)
    : memory_(static_cast<std::uint64_t>(width) * height * 4 * 3), api_(api),
      pixels_(static_cast<std::size_t>(width) * height * 4), width_(width), height_(height),
      pixelFormat_(format == static_cast<std::uint32_t>(StreamPixelFormat::Bgra32) ? 0x80E1 : GL_RGBA)
{
    if (!api_.bindBuffer) throw std::runtime_error("CPU fallback requires OpenGL pixel buffer binding support");
    D3D11_TEXTURE2D_DESC desc {};
    desc.Width = width; desc.Height = height; desc.ArraySize = 1; desc.MipLevels = 1;
    desc.Format = NativeFormat(format); desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CheckHr(device->CreateTexture2D(&desc, nullptr, &staging_), "Create external stream staging texture");
    CreateCompletionQuery(device, completion_);
    CheckGl("OpenGL error before CPU fallback initialization");
    UploadState state(api_);
    texture_.Create();
    glBindTexture(GL_TEXTURE_2D, texture_.Get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, pixelFormat_, GL_UNSIGNED_BYTE, nullptr);
    CheckGl("Cannot allocate CPU fallback OpenGL texture");
}
void CpuReadback::Begin(ID3D11DeviceContext* context, ID3D11Texture2D* source)
{
    context->CopyResource(staging_.Get(), source);
    context->End(completion_.Get());
    context->Flush();
}
bool CpuReadback::Ready(ID3D11DeviceContext* context) const
{
    const HRESULT result = context->GetData(completion_.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
    CheckHr(result, "Poll external stream readback completion");
    return result == S_OK;
}
bool CpuReadback::Upload(ID3D11DeviceContext* context)
{
    {
        StagingMap mapping(context, staging_.Get());
        if (!mapping.Mapped()) return false;
        const auto& mapped = mapping.Data();
        const std::size_t rowBytes = static_cast<std::size_t>(width_) * 4;
        if (!mapped.pData || mapped.RowPitch < rowBytes ||
            mapped.RowPitch > std::numeric_limits<std::size_t>::max() / height_)
            throw std::runtime_error("Invalid external stream staging row pitch");
        const auto* source = static_cast<const unsigned char*>(mapped.pData);
        for (std::size_t row = 0; row < height_; ++row)
            std::memcpy(pixels_.data() + row * rowBytes, source + row * mapped.RowPitch, rowBytes);
    }
    CheckGl("OpenGL error before CPU fallback upload");
    UploadState state(api_);
    glBindTexture(GL_TEXTURE_2D, texture_.Get());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_),
                    pixelFormat_, GL_UNSIGNED_BYTE, pixels_.data());
    CheckGl("CPU fallback OpenGL upload failed");
    return true;
}
} // namespace mfd::stream::detail
#endif
