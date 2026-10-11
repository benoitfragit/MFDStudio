#include "StreamGeneration.h"
#if defined(_WIN32)
namespace mfd::stream::detail
{
Generation::Generation(const GlInterop& api, ID3D11Device* device, const std::uint32_t width,
                       const std::uint32_t height, const std::uint32_t format, const std::uint64_t number)
    : memory_(static_cast<std::uint64_t>(width) * height * 4 * kSlotCount), api_(api),
      width_(width), height_(height), format_(format), number_(number)
{
    if (!wglGetCurrentContext() || !api_.SyncAvailable())
        throw UnsupportedDisplay("Unsupported: external streams require a current OpenGL context with sync support");
    try
    {
        D3D11_TEXTURE2D_DESC desc {};
        desc.Width = width; desc.Height = height; desc.ArraySize = 1; desc.MipLevels = 1;
        desc.Format = NativeFormat(format); desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        // The unchanged legacy DXGI contract uses GPU queries and a CPU metadata mutex.
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        for (std::size_t index = 0; index < kSlotCount; ++index)
        {
            CheckHr(device->CreateTexture2D(&desc, nullptr, &textures_[index]), "Create shared D3D11 texture");
            ComPtr<IDXGIResource> resource;
            CheckHr(textures_[index].As(&resource), "Get shared texture resource");
            HANDLE share = nullptr;
            CheckHr(resource->GetSharedHandle(&share), "Get shared D3D11 texture handle");
            // Legacy DXGI tokens are zero-extended 32-bit values, not closeable NT handles.
            handles_[index] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(share));
        }
        if (InitializeInterop(device)) diagnostic_ = "Direct GPU interop";
        else EnableCpu(device);
    }
    catch (...) { ReleaseInterop(false); throw; }
}
Generation::~Generation() { ReleaseInterop(false); }
bool Generation::InitializeInterop(ID3D11Device* device)
{
    diagnostic_ = "WGL_NV_DX_interop2 unavailable";
    if (!api_.Available()) return false;
    bridge_ = api_.open(device);
    diagnostic_ = "WGL device opening failed";
    if (!bridge_) return false;
    for (std::uint32_t index = 0; index < kSlotCount; ++index)
    {
        names_[index].Create();
        objects_[index] = api_.registerObject(bridge_, textures_[index].Get(), names_[index].Get(), GL_TEXTURE_2D, 0);
        diagnostic_ = "WGL texture registration failed";
        if (!objects_[index]) return false;
        diagnostic_ = "WGL texture ownership acquisition failed";
        if (!api_.lock(bridge_, 1, &objects_[index])) return false;
        locked_ = index;
        SetTextureParameters(index);
        // Failure to relinquish ownership is fatal: D3D access cannot safely follow it.
        Unlock();
    }
    return true;
}
void Generation::EnableCpu(ID3D11Device* device)
{
    interopRejected_ = true;
    CheckHr(device->GetDeviceRemovedReason(), "Fatal external stream D3D11 device loss");
    ReleaseInterop(true);
    try
    {
        readback_ = std::make_unique<CpuReadback>(api_, device, width_, height_, format_);
    }
    catch (const BudgetUnavailable&) { throw; }
    catch (const std::exception& error)
    {
        CheckHr(device->GetDeviceRemovedReason(), "Fatal external stream D3D11 device loss");
        throw UnsupportedDisplay("Unsupported: " + diagnostic_ + "; CPU fallback failed: " + error.what());
    }
    diagnostic_ = "CPU fallback: " + diagnostic_;
}
void Generation::SetTextureParameters(const std::uint32_t index)
{
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glBindTexture(GL_TEXTURE_2D, names_[index].Get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
}
void Generation::Acquire(const std::uint32_t index, ID3D11Device* device)
{
    if (readback_) return;
    if (!interopRejected_ && api_.lock(bridge_, 1, &objects_[index]))
    {
        locked_ = index;
        SetTextureParameters(index);
        return;
    }
    diagnostic_ = "WGL texture ownership acquisition failed";
    EnableCpu(device);
}
void Generation::Unlock()
{
    if (locked_ == kNoSlot) return;
    if (!api_.unlock(bridge_, 1, &objects_[locked_]))
        throw std::runtime_error("Cannot release WGL texture ownership");
    locked_ = kNoSlot;
}
void Generation::ReleaseInterop(const bool checkErrors)
{
    if (!bridge_) return;
    if (locked_ < kSlotCount)
    {
        const bool released = api_.unlock(bridge_, 1, &objects_[locked_]) != FALSE;
        if (!released && checkErrors) throw std::runtime_error("Cannot release WGL texture ownership");
        locked_ = kNoSlot;
    }
    for (std::size_t index = 0; index < kSlotCount; ++index)
    {
        if (objects_[index])
        {
            const bool released = api_.unregisterObject(bridge_, objects_[index]) != FALSE;
            if (!released && checkErrors) throw std::runtime_error("Cannot unregister WGL texture safely");
            objects_[index] = nullptr;
        }
        names_[index].Reset();
    }
    const bool closed = api_.close(bridge_) != FALSE;
    if (!closed && checkErrors) throw std::runtime_error("Cannot close WGL interop device safely");
    bridge_ = nullptr;
}
void Generation::BeginCopy(ID3D11DeviceContext* context, const std::uint32_t index)
{ readback_->Begin(context, textures_[index].Get()); }
bool Generation::CopyReady(ID3D11DeviceContext* context) const { return readback_->Ready(context); }
bool Generation::UploadCopy(ID3D11DeviceContext* context) { return readback_->Upload(context); }
GLuint Generation::Texture() const noexcept
{
    if (readback_) return readback_->Texture();
    return locked_ < kSlotCount ? names_[locked_].Get() : 0;
}
} // namespace mfd::stream::detail
#endif
