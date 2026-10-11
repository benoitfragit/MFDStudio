#include "StreamGl.h"
#include <cstring>
#include <stdexcept>
#if defined(_WIN32)
namespace mfd::stream::detail
{
namespace
{
PROC LoadGlFunction(const char* name) noexcept
{
    const PROC function = wglGetProcAddress(name);
    // WGL drivers may return these documented failure sentinels instead of nullptr.
    const auto address = reinterpret_cast<std::uintptr_t>(function);
    return address <= 3 || address == UINTPTR_MAX ? nullptr : function;
}
bool HasInteropExtension()
{
    using GetExtensions = const char*(WINAPI*)(HDC);
    const auto getExtensions = reinterpret_cast<GetExtensions>(LoadGlFunction("wglGetExtensionsStringARB"));
    const char* extensions = getExtensions ? getExtensions(wglGetCurrentDC()) : nullptr;
    if (!extensions) return false;
    constexpr char required[] = "WGL_NV_DX_interop2";
    const char* match = std::strstr(extensions, required);
    return match && (match == extensions || match[-1] == ' ') &&
           (match[sizeof(required) - 1] == ' ' || match[sizeof(required) - 1] == '\0');
}
#if defined(MFD_STREAM_TEST_HOOKS)
RegisterObject testRegistration = nullptr;
LockObjects testLock = nullptr;
unsigned int registrationCalls = 0;
unsigned int lockCalls = 0;
HANDLE WINAPI RejectOpen(void*) { return nullptr; }
HANDLE WINAPI RejectRegistration(HANDLE device, void* object, GLuint name, GLenum type, GLenum access)
{
    // Fail after a successful registration to exercise partial-resource cleanup.
    if (++registrationCalls % 2 == 0) return nullptr;
    return testRegistration(device, object, name, type, access);
}
BOOL WINAPI RejectLock(HANDLE, GLint, HANDLE*) { return FALSE; }
BOOL WINAPI RejectRuntimeLock(HANDLE device, GLint count, HANDLE* objects)
{
    // Three initialization probes and the first displayed frame succeed.
    if (++lockCalls > 4) return FALSE;
    return testLock(device, count, objects);
}
#endif
} // namespace
GlInterop::GlInterop()
{
    extensionAvailable = HasInteropExtension();
    open = reinterpret_cast<OpenDevice>(LoadGlFunction("wglDXOpenDeviceNV"));
    close = reinterpret_cast<CloseDevice>(LoadGlFunction("wglDXCloseDeviceNV"));
    registerObject = reinterpret_cast<RegisterObject>(LoadGlFunction("wglDXRegisterObjectNV"));
    unregisterObject = reinterpret_cast<UnregisterObject>(LoadGlFunction("wglDXUnregisterObjectNV"));
    lock = reinterpret_cast<LockObjects>(LoadGlFunction("wglDXLockObjectsNV"));
    unlock = reinterpret_cast<LockObjects>(LoadGlFunction("wglDXUnlockObjectsNV"));
    fence = reinterpret_cast<FenceSync>(LoadGlFunction("glFenceSync"));
    wait = reinterpret_cast<ClientWaitSync>(LoadGlFunction("glClientWaitSync"));
    deleteFence = reinterpret_cast<DeleteSync>(LoadGlFunction("glDeleteSync"));
    bindBuffer = reinterpret_cast<BindBuffer>(LoadGlFunction("glBindBuffer"));

#if defined(MFD_STREAM_TEST_HOOKS)
    // Compiled only into the hardware-test library; no production environment switch.
    char mode[32] {};
    GetEnvironmentVariableA("MFD_STREAM_TEST_INTEROP", mode, sizeof(mode));
    if (std::strcmp(mode, "cpu") == 0) extensionAvailable = false;
    if (std::strcmp(mode, "open") == 0) open = RejectOpen;
    if (std::strcmp(mode, "register") == 0)
    { testRegistration = registerObject; registerObject = RejectRegistration; }
    if (std::strcmp(mode, "lock") == 0) lock = RejectLock;
    if (std::strcmp(mode, "runtime-lock") == 0)
    { testLock = lock; lock = RejectRuntimeLock; }
    if (std::strcmp(mode, "unsupported") == 0)
    { extensionAvailable = false; bindBuffer = nullptr; }
#endif
}
bool GlInterop::Available() const noexcept
{
    return extensionAvailable && open && close && registerObject && unregisterObject &&
           lock && unlock && SyncAvailable();
}
bool GlInterop::SyncAvailable() const noexcept { return fence && wait && deleteFence; }
GlTexture::~GlTexture() { Reset(); }
void GlTexture::Create()
{
    glGenTextures(1, &name_);
    if (!name_) throw std::runtime_error("Cannot allocate external stream OpenGL texture");
}
void GlTexture::Reset() noexcept
{
    if (name_) glDeleteTextures(1, &name_);
    name_ = 0;
}
} // namespace mfd::stream::detail
#endif
