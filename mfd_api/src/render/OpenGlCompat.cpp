/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for OpenGlCompat.
 */

#include "OpenGlCompat.h"

#include <initializer_list>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#endif

#include <rlgl.h>

namespace mfd::detail
{
#if defined(_WIN32)
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_STENCIL_TEST
#define GL_STENCIL_TEST 0x0B90
#endif
#ifndef GL_STENCIL_BUFFER_BIT
#define GL_STENCIL_BUFFER_BIT 0x00000400
#endif

using GlGenRenderbuffersProc = void(APIENTRY*)(GLsizei, GLuint*);
using GlBindRenderbufferProc = void(APIENTRY*)(GLenum, GLuint);
using GlRenderbufferStorageProc = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
using GlDeleteRenderbuffersProc = void(APIENTRY*)(GLsizei, const GLuint*);
using GlStencilMaskProc = void(APIENTRY*)(GLuint);
using GlClearStencilProc = void(APIENTRY*)(GLint);
using GlColorMaskProc = void(APIENTRY*)(GLboolean, GLboolean, GLboolean, GLboolean);
using GlStencilFuncProc = void(APIENTRY*)(GLenum, GLint, GLuint);
using GlStencilOpProc = void(APIENTRY*)(GLenum, GLenum, GLenum);

struct GlApi
{
    GlGenRenderbuffersProc genRenderbuffers = nullptr;
    GlBindRenderbufferProc bindRenderbuffer = nullptr;
    GlRenderbufferStorageProc renderbufferStorage = nullptr;
    GlDeleteRenderbuffersProc deleteRenderbuffers = nullptr;
    GlStencilMaskProc stencilMask = nullptr;
    GlClearStencilProc clearStencil = nullptr;
    GlColorMaskProc colorMask = nullptr;
    GlStencilFuncProc stencilFunc = nullptr;
    GlStencilOpProc stencilOp = nullptr;
};

void* LoadGlProcAddress(const char* name) noexcept
{
    void* address = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (address != nullptr &&
        address != reinterpret_cast<void*>(0x1) &&
        address != reinterpret_cast<void*>(0x2) &&
        address != reinterpret_cast<void*>(0x3) &&
        address != reinterpret_cast<void*>(-1))
    {
        return address;
    }

    static HMODULE openglModule = GetModuleHandleA("opengl32.dll");
    return openglModule == nullptr ? nullptr : reinterpret_cast<void*>(GetProcAddress(openglModule, name));
}

void* LoadFirstGlProcAddress(const std::initializer_list<const char*> names) noexcept
{
    for (const char* name : names)
    {
        if (void* address = LoadGlProcAddress(name); address != nullptr)
        {
            return address;
        }
    }

    return nullptr;
}

const GlApi& GetGlApi() noexcept
{
    static const GlApi api = []()
    {
        GlApi loaded;
        loaded.genRenderbuffers =
            reinterpret_cast<GlGenRenderbuffersProc>(LoadFirstGlProcAddress({"glGenRenderbuffers",
                                                                             "glGenRenderbuffersEXT"}));
        loaded.bindRenderbuffer =
            reinterpret_cast<GlBindRenderbufferProc>(LoadFirstGlProcAddress({"glBindRenderbuffer",
                                                                             "glBindRenderbufferEXT"}));
        loaded.renderbufferStorage =
            reinterpret_cast<GlRenderbufferStorageProc>(LoadFirstGlProcAddress({"glRenderbufferStorage",
                                                                                "glRenderbufferStorageEXT"}));
        loaded.deleteRenderbuffers =
            reinterpret_cast<GlDeleteRenderbuffersProc>(LoadFirstGlProcAddress({"glDeleteRenderbuffers",
                                                                                "glDeleteRenderbuffersEXT"}));
        loaded.stencilMask = reinterpret_cast<GlStencilMaskProc>(LoadGlProcAddress("glStencilMask"));
        loaded.clearStencil = reinterpret_cast<GlClearStencilProc>(LoadGlProcAddress("glClearStencil"));
        loaded.colorMask = reinterpret_cast<GlColorMaskProc>(LoadGlProcAddress("glColorMask"));
        loaded.stencilFunc = reinterpret_cast<GlStencilFuncProc>(LoadGlProcAddress("glStencilFunc"));
        loaded.stencilOp = reinterpret_cast<GlStencilOpProc>(LoadGlProcAddress("glStencilOp"));
        return loaded;
    }();

    return api;
}

GLenum ToGlCompare(const GlStencilCompare compare) noexcept
{
    switch (compare)
    {
    case GlStencilCompare::Equal:
        return GL_EQUAL;
    case GlStencilCompare::NotEqual:
        return GL_NOTEQUAL;
    case GlStencilCompare::Always:
    default:
        return GL_ALWAYS;
    }
}

GLenum ToGlOperation(const GlStencilOperation operation) noexcept
{
    switch (operation)
    {
    case GlStencilOperation::Replace:
        return GL_REPLACE;
    case GlStencilOperation::Keep:
    default:
        return GL_KEEP;
    }
}

bool OpenGlDepthStencilRenderbufferApiAvailable() noexcept
{
#if defined(_WIN32)
    const GlApi& gl = GetGlApi();
    return gl.genRenderbuffers != nullptr &&
           gl.bindRenderbuffer != nullptr &&
           gl.renderbufferStorage != nullptr &&
           gl.deleteRenderbuffers != nullptr;
#else
    return false;
#endif
}
#endif

bool OpenGlStencilApiAvailable() noexcept
{
#if defined(_WIN32)
    const GlApi& gl = GetGlApi();
    return gl.stencilMask != nullptr &&
           gl.clearStencil != nullptr &&
           gl.colorMask != nullptr &&
           gl.stencilFunc != nullptr &&
           gl.stencilOp != nullptr;
#else
    return false;
#endif
}

void OpenGlSetStencilEnabled([[maybe_unused]] const bool enabled) noexcept
{
#if defined(_WIN32)
    if (enabled)
    {
        glEnable(GL_STENCIL_TEST);
    }
    else
    {
        glDisable(GL_STENCIL_TEST);
    }
#endif
}

void OpenGlSetStencilMask([[maybe_unused]] const std::uint32_t mask) noexcept
{
#if defined(_WIN32)
    if (const GlApi& gl = GetGlApi(); gl.stencilMask != nullptr)
    {
        gl.stencilMask(mask);
    }
#endif
}

void OpenGlClearStencilValue([[maybe_unused]] const int value) noexcept
{
#if defined(_WIN32)
    if (const GlApi& gl = GetGlApi(); gl.clearStencil != nullptr)
    {
        gl.clearStencil(value);
    }
#endif
}

void OpenGlClearStencilBuffer() noexcept
{
#if defined(_WIN32)
    glClear(GL_STENCIL_BUFFER_BIT);
#endif
}

void OpenGlSetColorWriteMask([[maybe_unused]] const bool red,
                             [[maybe_unused]] const bool green,
                             [[maybe_unused]] const bool blue,
                             [[maybe_unused]] const bool alpha) noexcept
{
#if defined(_WIN32)
    if (const GlApi& gl = GetGlApi(); gl.colorMask != nullptr)
    {
        gl.colorMask(red ? GL_TRUE : GL_FALSE,
                     green ? GL_TRUE : GL_FALSE,
                     blue ? GL_TRUE : GL_FALSE,
                     alpha ? GL_TRUE : GL_FALSE);
    }
#endif
}

void OpenGlSetStencilFunction([[maybe_unused]] const GlStencilCompare compare,
                              [[maybe_unused]] const int reference,
                              [[maybe_unused]] const std::uint32_t mask) noexcept
{
#if defined(_WIN32)
    if (const GlApi& gl = GetGlApi(); gl.stencilFunc != nullptr)
    {
        gl.stencilFunc(ToGlCompare(compare), reference, mask);
    }
#endif
}

void OpenGlSetStencilOperation([[maybe_unused]] const GlStencilOperation stencilFail,
                               [[maybe_unused]] const GlStencilOperation depthFail,
                               [[maybe_unused]] const GlStencilOperation depthPass) noexcept
{
#if defined(_WIN32)
    if (const GlApi& gl = GetGlApi(); gl.stencilOp != nullptr)
    {
        gl.stencilOp(ToGlOperation(stencilFail), ToGlOperation(depthFail), ToGlOperation(depthPass));
    }
#endif
}

bool OpenGlReplaceDepthRenderbufferWithDepthStencil([[maybe_unused]] const unsigned int framebufferId,
                                                    [[maybe_unused]] const unsigned int previousDepthId,
                                                    [[maybe_unused]] const int width,
                                                    [[maybe_unused]] const int height,
                                                    [[maybe_unused]] unsigned int* replacementRenderbufferId) noexcept
{
#if defined(_WIN32)
    if (replacementRenderbufferId == nullptr || !OpenGlDepthStencilRenderbufferApiAvailable())
    {
        return false;
    }

    const GlApi& gl = GetGlApi();
    unsigned int depthStencilId = 0;
    gl.genRenderbuffers(1, &depthStencilId);
    if (depthStencilId == 0)
    {
        return false;
    }

    gl.bindRenderbuffer(GL_RENDERBUFFER, depthStencilId);
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    gl.bindRenderbuffer(GL_RENDERBUFFER, 0);

    rlEnableFramebuffer(framebufferId);
    rlFramebufferAttach(framebufferId, depthStencilId, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    rlFramebufferAttach(framebufferId, depthStencilId, RL_ATTACHMENT_STENCIL, RL_ATTACHMENT_RENDERBUFFER, 0);
    const bool framebufferComplete = rlFramebufferComplete(framebufferId);
    rlDisableFramebuffer();

    if (!framebufferComplete)
    {
        rlEnableFramebuffer(framebufferId);
        rlFramebufferAttach(framebufferId, previousDepthId, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
        rlFramebufferAttach(framebufferId, 0, RL_ATTACHMENT_STENCIL, RL_ATTACHMENT_RENDERBUFFER, 0);
        rlDisableFramebuffer();
        gl.deleteRenderbuffers(1, &depthStencilId);
        return false;
    }

    *replacementRenderbufferId = depthStencilId;
    gl.deleteRenderbuffers(1, &previousDepthId);
    return true;
#else
    return false;
#endif
}
} // namespace mfd::detail
