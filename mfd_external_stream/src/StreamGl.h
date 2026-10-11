#pragma once
#include "NativeStream.h"
#if defined(_WIN32)
#include <GL/gl.h>
namespace mfd::stream::detail
{
using OpenDevice = HANDLE(WINAPI*)(void*);
using CloseDevice = BOOL(WINAPI*)(HANDLE);
using RegisterObject = HANDLE(WINAPI*)(HANDLE, void*, GLuint, GLenum, GLenum);
using UnregisterObject = BOOL(WINAPI*)(HANDLE, HANDLE);
using LockObjects = BOOL(WINAPI*)(HANDLE, GLint, HANDLE*);
using FenceSync = void*(APIENTRY*)(GLenum, GLbitfield);
using ClientWaitSync = GLenum(APIENTRY*)(void*, GLbitfield, std::uint64_t);
using DeleteSync = void(APIENTRY*)(void*);
using BindBuffer = void(APIENTRY*)(GLenum, GLuint);
/** @brief Owns a texture name in the current context. */
class GlTexture
{
public:
    GlTexture() = default;
    ~GlTexture();
    GlTexture(const GlTexture&) = delete;
    GlTexture& operator=(const GlTexture&) = delete;
    void Create();
    void Reset() noexcept;
    GLuint Get() const noexcept { return name_; }
private:
    GLuint name_ = 0;
};
/** @brief Context-local native entry points; casts are confined to the WGL loader. */
struct GlInterop
{
    OpenDevice open = nullptr;
    CloseDevice close = nullptr;
    RegisterObject registerObject = nullptr;
    UnregisterObject unregisterObject = nullptr;
    LockObjects lock = nullptr;
    LockObjects unlock = nullptr;
    FenceSync fence = nullptr;
    ClientWaitSync wait = nullptr;
    DeleteSync deleteFence = nullptr;
    BindBuffer bindBuffer = nullptr;
    bool extensionAvailable = false;
    GlInterop();
    bool Available() const noexcept;
    bool SyncAvailable() const noexcept;
};
} // namespace mfd::stream::detail
#endif
