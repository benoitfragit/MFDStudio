/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Stable C ABI used by framebuffer-processing plugins loaded by `mfd_window`.
 *
 * @details This boundary intentionally avoids C++ classes, STL containers,
 * exceptions, and host-owned graphics objects across the DLL edge. Plugins
 * receive one raw frame descriptor in the pixel layout they requested and
 * implement an explicit `init / submit_frame / close / destroy` lifecycle.
 */

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#    define MFD_WINDOW_PLUGIN_CALL __cdecl
#    if defined(MFD_WINDOW_PLUGIN_API_EXPORTS)
#        define MFD_WINDOW_PLUGIN_API __declspec(dllexport)
#    elif defined(MFD_WINDOW_PLUGIN_API_DLL)
#        define MFD_WINDOW_PLUGIN_API __declspec(dllimport)
#    else
#        define MFD_WINDOW_PLUGIN_API
#    endif
#else
#    define MFD_WINDOW_PLUGIN_CALL
#    if defined(__GNUC__) || defined(__clang__)
#        define MFD_WINDOW_PLUGIN_API __attribute__((visibility("default")))
#    else
#        define MFD_WINDOW_PLUGIN_API
#    endif
#endif

#if defined(__cplusplus)
#    define MFD_WINDOW_PLUGIN_NOEXCEPT noexcept
#else
#    define MFD_WINDOW_PLUGIN_NOEXCEPT
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief ABI revision implemented by the framebuffer plugin contract. */
enum
{
    MFD_WINDOW_FRAMEBUFFER_PLUGIN_ABI_VERSION = 3u
};

/** @brief Exported symbol name returning the framebuffer plugin callback table. */
#define MFD_WINDOW_FRAMEBUFFER_PLUGIN_ENTRY_POINT "MfdGetWindowFramebufferPluginApi"

/**
 * @brief Stable status codes exchanged across the framebuffer plugin ABI.
 */
typedef enum MfdWindowFramebufferPluginResultCode
{
    MfdWindowFramebufferPluginResultCode_Success = 0,
    MfdWindowFramebufferPluginResultCode_BufferTooSmall = 1,
    MfdWindowFramebufferPluginResultCode_InvalidArgument = 2,
    MfdWindowFramebufferPluginResultCode_InvalidState = 3,
    MfdWindowFramebufferPluginResultCode_Unsupported = 4,
    MfdWindowFramebufferPluginResultCode_InternalFailure = 5,
    MfdWindowFramebufferPluginResultCode_AbiMismatch = 6
} MfdWindowFramebufferPluginResultCode;

/**
 * @brief Raw pixel layout identifiers supported by the framebuffer plugin ABI.
 */
typedef enum MfdWindowFramebufferPixelFormat
{
    /** @brief Tightly packed `RGBA32`, one byte per component. */
    MfdWindowFramebufferPixelFormat_Rgba32 = 1,
    /** @brief Tightly packed `BGRA32`, one byte per component, compatible with `GL_BGRA_EXT`. */
    MfdWindowFramebufferPixelFormat_Bgra32 = 2
} MfdWindowFramebufferPixelFormat;

/**
 * @brief Borrowed UTF-8 string slice crossing the plugin ABI.
 *
 * @note The caller owns the referenced memory.
 */
typedef struct MfdWindowStringView
{
    const char* data;
    size_t size;
} MfdWindowStringView;

/**
 * @brief Mutable UTF-8 output buffer used to return human-readable errors.
 *
 * @note Implementations should write at most `capacity - 1` bytes, terminate
 * the buffer with `\0` when `capacity > 0`, and set `size` to the full text
 * length excluding the trailing null terminator.
 */
typedef struct MfdWindowUtf8Buffer
{
    char* data;
    size_t capacity;
    size_t size;
} MfdWindowUtf8Buffer;

/**
 * @brief One raw framebuffer delivered by the host to one plugin.
 */
typedef struct MfdWindowFramebufferFrame
{
    size_t struct_size;
    uint32_t pixel_format;
    int32_t width;
    int32_t height;
    size_t row_stride_bytes;
    const uint8_t* pixels;
    size_t pixel_bytes;
} MfdWindowFramebufferFrame;

/**
 * @brief Versioned host descriptor passed to `init`.
 *
 * @details The host can append optional callbacks in future ABI revisions while
 * preserving the leading layout identified by `struct_size`.
 */
typedef struct MfdWindowFramebufferPluginHostApi
{
    size_t struct_size;
    uint32_t abi_version;
    /** @brief Pixel format selected by the host for `submit_frame`. */
    uint32_t output_pixel_format;
    /** @brief Number of process launch arguments forwarded from `mfd_window`. */
    int32_t launch_argc;
    /**
     * @brief Borrowed UTF-8 launch argument array forwarded from `mfd_window`.
     *
     * @note The array and string pointers must be treated as read-only. Plugins
     * should copy any value that must outlive their loaded instance.
     */
    const char* const* launch_argv;
} MfdWindowFramebufferPluginHostApi;

/**
 * @brief Immutable metadata describing one framebuffer plugin instance.
 */
typedef struct MfdWindowFramebufferPluginInfo
{
    size_t struct_size;
    uint32_t abi_version;
    MfdWindowStringView plugin_id;
    MfdWindowStringView display_name;
    /** @brief Pixel format requested by the plugin for `submit_frame`. */
    uint32_t requested_pixel_format;
} MfdWindowFramebufferPluginInfo;

/**
 * @brief Versioned callback table exported by one framebuffer plugin DLL.
 */
typedef struct MfdWindowFramebufferPluginApi
{
    size_t struct_size;
    MfdWindowFramebufferPluginInfo info;
    void* plugin_context;
    MfdWindowFramebufferPluginResultCode(MFD_WINDOW_PLUGIN_CALL* init)(
        void* plugin_context,
        const MfdWindowFramebufferPluginHostApi* host,
        MfdWindowUtf8Buffer* error);
    MfdWindowFramebufferPluginResultCode(MFD_WINDOW_PLUGIN_CALL* submit_frame)(
        void* plugin_context,
        const MfdWindowFramebufferFrame* frame,
        MfdWindowUtf8Buffer* error);
    void (MFD_WINDOW_PLUGIN_CALL* close)(void* plugin_context);
    void (MFD_WINDOW_PLUGIN_CALL* destroy)(void* plugin_context);
} MfdWindowFramebufferPluginApi;

/**
 * @brief Function type filling one stable framebuffer plugin descriptor.
 */
typedef MfdWindowFramebufferPluginResultCode(MFD_WINDOW_PLUGIN_CALL* MfdGetWindowFramebufferPluginApiFn)(
    MfdWindowFramebufferPluginApi* out_api,
    MfdWindowUtf8Buffer* error);

/**
 * @brief Computes the expected byte count for one tightly packed framebuffer layout.
 * @param pixel_format Raw pixel layout requested by the plugin ABI.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @return Expected byte count, or `0` when the format or dimensions are invalid or overflow.
 */
MFD_WINDOW_PLUGIN_API size_t MFD_WINDOW_PLUGIN_CALL MfdWindowComputeFramebufferByteCount(
    uint32_t pixel_format,
    int32_t width,
    int32_t height) MFD_WINDOW_PLUGIN_NOEXCEPT;

/**
 * @brief Validates that one raw byte buffer matches one tightly packed framebuffer layout.
 * @param pixel_format Raw pixel layout requested by the plugin ABI.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @param pixels Pointer to the first raw pixel byte.
 * @param pixel_bytes Raw byte count available behind `pixels`.
 * @return Non-zero when the buffer matches one complete tightly packed frame in the requested format.
 */
MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferLayout(
    uint32_t pixel_format,
    int32_t width,
    int32_t height,
    const void* pixels,
    size_t pixel_bytes) MFD_WINDOW_PLUGIN_NOEXCEPT;

/**
 * @brief Computes the expected `RGBA32` byte count for one framebuffer.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @return Expected byte count, or `0` when the dimensions are invalid or overflow.
 */
MFD_WINDOW_PLUGIN_API size_t MFD_WINDOW_PLUGIN_CALL MfdWindowComputeFramebufferRgba32ByteCount(
    int32_t width,
    int32_t height) MFD_WINDOW_PLUGIN_NOEXCEPT;

/**
 * @brief Validates that one raw byte buffer matches one tightly packed `RGBA32` frame.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @param pixels Pointer to the first raw pixel byte.
 * @param pixel_bytes Raw byte count available behind `pixels`.
 * @return Non-zero when the buffer matches one complete `RGBA32` frame.
 */
MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferRgba32Layout(
    int32_t width,
    int32_t height,
    const void* pixels,
    size_t pixel_bytes) MFD_WINDOW_PLUGIN_NOEXCEPT;

/**
 * @brief Validates one `MfdWindowFramebufferFrame` instance received from the host.
 * @param frame Raw frame descriptor to validate.
 * @return Non-zero when the descriptor exposes one complete tightly packed `RGBA32` frame.
 */
MFD_WINDOW_PLUGIN_API int MFD_WINDOW_PLUGIN_CALL MfdWindowValidateFramebufferFrame(
    const MfdWindowFramebufferFrame* frame) MFD_WINDOW_PLUGIN_NOEXCEPT;

#ifdef __cplusplus
} // extern "C"

namespace mfd::window
{
/** @brief Exported symbol name searched inside a framebuffer plugin DLL. */
constexpr const char* kLauncherFramebufferPluginEntryPointName = MFD_WINDOW_FRAMEBUFFER_PLUGIN_ENTRY_POINT;

/**
 * @brief Computes the expected byte count for one tightly packed framebuffer layout.
 * @param pixelFormat Raw pixel layout requested by the plugin ABI.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @return Expected byte count, or `0` when the format or dimensions are invalid or overflow.
 */
inline size_t ComputeFramebufferByteCount(const MfdWindowFramebufferPixelFormat pixelFormat,
                                          const int width,
                                          const int height) noexcept
{
    return MfdWindowComputeFramebufferByteCount(static_cast<uint32_t>(pixelFormat), width, height);
}

/**
 * @brief Validates that one raw byte buffer matches one tightly packed framebuffer layout.
 * @param pixelFormat Raw pixel layout requested by the plugin ABI.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @param pixels Pointer to the first raw pixel byte.
 * @param pixelBytes Raw byte count available behind `pixels`.
 * @return `true` when the buffer matches one complete tightly packed frame in the requested format.
 */
inline bool ValidateFramebufferLayout(const MfdWindowFramebufferPixelFormat pixelFormat,
                                      const int width,
                                      const int height,
                                      const void* pixels,
                                      const size_t pixelBytes) noexcept
{
    return MfdWindowValidateFramebufferLayout(
               static_cast<uint32_t>(pixelFormat),
               width,
               height,
               pixels,
               pixelBytes) != 0;
}

/**
 * @brief Computes the expected `RGBA32` byte count for one framebuffer.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @return Expected byte count, or `0` when the dimensions are invalid or overflow.
 */
inline size_t ComputeFramebufferRgba32ByteCount(const int width, const int height) noexcept
{
    return ComputeFramebufferByteCount(MfdWindowFramebufferPixelFormat_Rgba32, width, height);
}

/**
 * @brief Validates that one raw byte buffer matches one tightly packed `RGBA32` frame.
 * @param width Framebuffer width in pixels.
 * @param height Framebuffer height in pixels.
 * @param pixels Pointer to the first raw pixel byte.
 * @param pixelBytes Raw byte count available behind `pixels`.
 * @return `true` when the buffer matches one complete `RGBA32` frame.
 */
inline bool ValidateFramebufferRgba32Layout(const int width,
                                            const int height,
                                            const void* pixels,
                                            const size_t pixelBytes) noexcept
{
    return ValidateFramebufferLayout(
               MfdWindowFramebufferPixelFormat_Rgba32,
               width,
               height,
               pixels,
               pixelBytes);
}

/**
 * @brief Validates one raw frame descriptor received from the host.
 * @param frame Raw frame descriptor to validate.
 * @return `true` when the descriptor exposes one complete tightly packed `RGBA32` frame.
 */
inline bool ValidateFramebufferFrame(const MfdWindowFramebufferFrame& frame) noexcept
{
    return MfdWindowValidateFramebufferFrame(&frame) != 0;
}
} // namespace mfd::window
#endif
