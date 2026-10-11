/** @file @brief Bounded real-process D3D11/WGL integration test; excluded only from hosted GPU-less CI. */
#include "mfd/stream/StreamProducer.h"
#include "mfd/stream/StreamReceiver.h"
#include "StreamBudget.h"
#include "StreamGl.h"
#include <windows.h>
#include <d3d11.h>
#include <GL/gl.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
void OpenTestContext();
void CloseTestContext();

namespace
{
bool childCleanupFailed = false;
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
class Child
{
public:
    explicit Child(const std::wstring& instance, const int variant = 0)
    {
        std::array<wchar_t, 32768> executable {};
        const DWORD overrideLength = GetEnvironmentVariableW(L"MFD_STREAM_TEST_PRODUCER", executable.data(),
                                                              static_cast<DWORD>(executable.size()));
        Require(overrideLength < executable.size(), "Producer executable path too long");
        if (overrideLength == 0)
            Require(GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size())) > 0, "Executable path");
        std::wstring command = L"\"" + std::wstring(executable.data()) + L"\" --producer " + instance + L" " + std::to_wstring(variant);
        STARTUPINFOW startup {}; startup.cb = sizeof(startup);
        Require(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &startup, &process_) != FALSE, "Create producer process");
    }
    ~Child()
    {
        if (WaitForSingleObject(process_.hProcess, 0) == WAIT_TIMEOUT) TerminateProcess(process_.hProcess, 1);
        if (WaitForSingleObject(process_.hProcess, 5000) != WAIT_OBJECT_0) childCleanupFailed = true;
        CloseHandle(process_.hThread); CloseHandle(process_.hProcess);
    }
    void Kill()
    {
        Require(TerminateProcess(process_.hProcess, 0) != FALSE, "Terminate producer");
        Require(WaitForSingleObject(process_.hProcess, 5000) == WAIT_OBJECT_0, "Producer termination deadline expired");
    }
private:
    PROCESS_INFORMATION process_ {};
};
void HoldControl(const HANDLE mutex, std::promise<bool> acquired, std::future<void> release)
{
    const DWORD result = WaitForSingleObject(mutex, 2000);
    acquired.set_value(result == WAIT_OBJECT_0);
    if (result == WAIT_OBJECT_0)
    {
        release.wait_for(std::chrono::seconds(5));
        ReleaseMutex(mutex);
    }
    else if (result == WAIT_ABANDONED) ReleaseMutex(mutex);
}
/** @brief Holds the protocol mutex from another thread; every wait has a finite test deadline. */
class ControlBlocker
{
public:
    explicit ControlBlocker(const HANDLE mutex)
        : acquiredResult_(acquired_.get_future()),
          worker_(HoldControl, mutex, std::move(acquired_), release_.get_future()) {}
    ~ControlBlocker() { release_.set_value(); worker_.join(); }
    ControlBlocker(const ControlBlocker&) = delete;
    ControlBlocker& operator=(const ControlBlocker&) = delete;
    bool Acquired()
    {
        return acquiredResult_.wait_for(std::chrono::seconds(3)) == std::future_status::ready && acquiredResult_.get();
    }
private:
    std::promise<bool> acquired_;
    std::promise<void> release_;
    std::future<bool> acquiredResult_;
    std::thread worker_;
};
int Produce(const std::string& instance, const bool bgra)
{
    mfd::stream::StreamProducer producer;
    const std::uint32_t width = bgra ? 32 : 64;
    const std::uint32_t height = bgra ? 64 : 32;
    std::vector<unsigned char> pixels(width * height * 4);
    for (std::uint64_t frame = 1; frame <= 1800; ++frame)
    {
        const auto opened = producer.Open(instance, "test", width, height, bgra ? mfd::StreamPixelFormat::Bgra32 : mfd::StreamPixelFormat::Rgba32);
        if (opened == mfd::StreamResult::Ok || opened == mfd::StreamResult::Reconfigured)
        {
            ID3D11Texture2D* texture = nullptr; ID3D11DeviceContext* context = nullptr;
            if (producer.TryAcquire(texture, context) == mfd::StreamResult::Ok)
            {
                for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4)
                {
                    const auto red = static_cast<unsigned char>(frame % 251);
                    const auto blue = static_cast<unsigned char>((pixel / 4) % width);
                    pixels[pixel] = bgra ? blue : red;
                    pixels[pixel + 1] = static_cast<unsigned char>((pixel / 4) / width);
                    pixels[pixel + 2] = bgra ? red : blue;
                    pixels[pixel + 3] = 127;
                }
                context->UpdateSubresource(texture, 0, nullptr, pixels.data(), width * 4, 0);
                if (producer.Publish(frame) != mfd::StreamResult::Ok) producer.Cancel();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}
void Tick(mfd::stream::StreamReceiver& receiver, mfd::StreamPlayback playback, std::uint64_t revision, bool visible)
{
    receiver.Update(playback, revision, visible);
    if (visible) receiver.EndFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
void CheckPixels(mfd::stream::StreamReceiver& receiver)
{
    Require(receiver.Texture() != 0, "Missing consumed texture");
    std::vector<unsigned char> pixels(receiver.Width() * receiver.Height() * 4);
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glBindTexture(GL_TEXTURE_2D, receiver.Texture());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    const auto expected = receiver.Status().consumedFrame % 251;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4)
        Require(pixels[pixel] == expected && pixels[pixel + 1] == (pixel / 4) / receiver.Width() &&
                pixels[pixel + 2] == (pixel / 4) % receiver.Width() && pixels[pixel + 3] == 127,
                "Torn frame or invalid RGBA channels");
}
/** @brief Exercises upload isolation from non-default renderer pixel-store settings. */
class UnpackSettings
{
public:
    UnpackSettings()
    {
        // Native GL entry-point casts are required by the Windows extension loader.
        const auto generate = reinterpret_cast<BufferNames>(wglGetProcAddress("glGenBuffers"));
        deleteBuffers_ = reinterpret_cast<DeleteBuffers>(wglGetProcAddress("glDeleteBuffers"));
        Require(generate && deleteBuffers_ && api_.bindBuffer, "Missing OpenGL buffer API");
        texture_.Create();
        generate(1, &buffer_);
        Require(buffer_ != 0, "Cannot create unpack buffer fixture");
        api_.bindBuffer(0x88EC, buffer_);
        glBindTexture(GL_TEXTURE_2D, texture_.Get());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 111);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
    }
    ~UnpackSettings()
    {
        api_.bindBuffer(0x88EC, 0);
        deleteBuffers_(1, &buffer_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }
    UnpackSettings(const UnpackSettings&) = delete;
    UnpackSettings& operator=(const UnpackSettings&) = delete;
    void Check() const
    {
        GLint value = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
        Require(static_cast<GLuint>(value) == texture_.Get(), "Upload changed texture binding");
        glGetIntegerv(0x88EF, &value);
        Require(static_cast<GLuint>(value) == buffer_, "Upload changed unpack buffer binding");
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &value); Require(value == 8, "Upload changed unpack alignment");
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &value); Require(value == 111, "Upload changed unpack row length");
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &value); Require(value == 2, "Upload changed unpack skip rows");
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &value); Require(value == 3, "Upload changed unpack skip pixels");
    }
private:
    using BufferNames = void(APIENTRY*)(GLsizei, GLuint*);
    using DeleteBuffers = void(APIENTRY*)(GLsizei, const GLuint*);
    mfd::stream::detail::GlInterop api_;
    mfd::stream::detail::GlTexture texture_;
    DeleteBuffers deleteBuffers_ = nullptr;
    GLuint buffer_ = 0;
};
void CheckStopBatch(mfd::stream::StreamReceiver& receiver)
{
    // A Stop followed by Play in one batch only changes the Stop revision seen here.
    Tick(receiver, mfd::StreamPlayback::Play, 2, true);
    Require(!receiver.Status().hasImage && receiver.Texture() == 0, "Stop/Play batch displayed an old-epoch copy");
    for (int attempt = 0; attempt < 100 && !receiver.Status().hasImage; ++attempt)
        Tick(receiver, mfd::StreamPlayback::Play, 2, true);
    CheckPixels(receiver);
}
/** @brief Takes a control snapshot without making timing assumptions about the producer mutex. */
mfd::stream::detail::WireHeader ReadControl(mfd::stream::detail::Endpoint& endpoint)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        mfd::stream::detail::TryLock lock(endpoint.Mutex());
        Require(!lock.Abandoned(), "Abandoned test control mutex");
        if (lock.Acquired()) return endpoint.Header();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Control snapshot deadline expired");
}
void CheckStopWithBusyControl(mfd::stream::StreamReceiver& receiver, const std::string& instance)
{
    mfd::stream::detail::Endpoint observer;
    observer.Open(instance, "test", false);
    const auto previous = ReadControl(observer);
    {
        ControlBlocker blocker(observer.Mutex());
        Require(blocker.Acquired(), "Could not hold the control mutex for the Stop regression");
        receiver.Update(mfd::StreamPlayback::Stop, 1, true);
        Require(!receiver.Status().hasImage && receiver.Texture() == 0, "Busy control mutex delayed visible Stop");
        receiver.Update(mfd::StreamPlayback::Play, 1, true);
        Require(!receiver.Status().hasImage && receiver.Texture() == 0, "Busy Stop/Play resurrected the old image");
    }
    for (int attempt = 0; attempt < 100 && ReadControl(observer).epoch == previous.epoch; ++attempt)
        Tick(receiver, mfd::StreamPlayback::Pause, 1, true);
    Require(ReadControl(observer).epoch > previous.epoch, "Busy Stop/Play lost its deferred epoch invalidation");
    for (int attempt = 0; attempt < 100 && !receiver.Status().hasImage; ++attempt)
        Tick(receiver, mfd::StreamPlayback::Play, 1, true);
    CheckPixels(receiver);
}
void CheckUnchangedGeneration(const mfd::stream::detail::WireHeader& previous,
                              const mfd::stream::detail::WireHeader& rejected)
{
    Require(rejected.generation == previous.generation && rejected.width == previous.width &&
            rejected.height == previous.height && rejected.format == previous.format,
            "Rejected generation changed published configuration");
    for (std::size_t index = 0; index < mfd::stream::detail::kSlotCount; ++index)
        Require(rejected.slots[index].handle == previous.slots[index].handle,
                "Rejected generation published a partial shared handle set");
}
bool HasHardware(const std::string& mode)
{
    if (!wglGetCurrentContext()) { std::puts("SKIP: no OpenGL context"); return false; }
    mfd::stream::detail::GlInterop api;
    if (!api.SyncAvailable()) { std::puts("SKIP: no OpenGL sync support"); return false; }
    if (mode != "cpu" && mode != "unsupported" && !api.Available())
    { std::puts("SKIP: direct WGL entry points unavailable for this interop test"); return false; }
    try
    {
        mfd::stream::detail::ComPtr<ID3D11Device> device;
        mfd::stream::detail::ComPtr<ID3D11DeviceContext> context;
        mfd::stream::detail::CreateDevice(device, context);
    }
    catch (const std::exception& error) { std::printf("SKIP: %s\n", error.what()); return false; }
    return true;
}
void CheckUnsupportedDisplay()
{
    const auto initialBytes = mfd::stream::detail::ReservedStreamBytes();
    const auto instance = mfd::stream::NewStreamInstance();
    mfd::stream::StreamReceiver receiver(instance, "test");
    Child producer(std::wstring(instance.begin(), instance.end()));
    for (int attempt = 0; attempt < 100 && receiver.Status().result != mfd::StreamResult::Unsupported; ++attempt)
        Tick(receiver, mfd::StreamPlayback::Play, 0, true);
    Require(receiver.Status().result == mfd::StreamResult::Unsupported, "Unavailable display paths were not diagnosed");
    const auto diagnostic = receiver.Status().diagnostic;
    Require(diagnostic.find("CPU fallback failed") != std::string::npos, "Missing fallback failure reason");
    for (int attempt = 0; attempt < 100; ++attempt) receiver.Update(mfd::StreamPlayback::Play, 0, true);
    Require(receiver.Status().diagnostic == diagnostic && !receiver.Status().hasImage, "Terminal display error was retried");
    Require(mfd::stream::detail::ReservedStreamBytes() == initialBytes, "Unsupported generation leaked resources");
    std::puts("PASS: unsupported display stops retries and releases partial resources");
}
int Consume(const std::string& mode)
{
    OpenTestContext();
    if (!HasHardware(mode)) { CloseTestContext(); return 77; }
    SetEnvironmentVariableA("MFD_STREAM_TEST_INTEROP", mode.c_str());
    int result = 0;
    const auto initialBytes = mfd::stream::detail::ReservedStreamBytes();
    try
    {
        if (mode == "unsupported")
        {
            CheckUnsupportedDisplay();
            Require(!childCleanupFailed, "Producer teardown deadline expired");
            SetEnvironmentVariableA("MFD_STREAM_TEST_INTEROP", nullptr);
            CloseTestContext();
            return 0;
        }
        UnpackSettings unpack;
        const std::string instance = mfd::stream::NewStreamInstance();
        mfd::stream::StreamReceiver receiver(instance, "test");
        Child producer(std::wstring(instance.begin(), instance.end()));
        {
            mfd::stream::detail::MemoryReservation pressure(mfd::stream::detail::kStreamMemoryBudget - initialBytes);
            // Process/device startup can exceed 400 ms under concurrent GPU load.
            for (int attempt = 0; attempt < 300 && receiver.Status().result != mfd::StreamResult::Unavailable; ++attempt)
                Tick(receiver, mfd::StreamPlayback::Play, 0, true);
            Require(receiver.Status().result == mfd::StreamResult::Unavailable && !receiver.Status().hasImage,
                    "Initial budget pressure became fatal or allocated unaccounted resources");
        }
        if (!mode.empty() && mode != "runtime-lock")
        {
            constexpr std::uint64_t sharedBytes = 64 * 32 * 4 * mfd::stream::detail::kSlotCount;
            mfd::stream::detail::MemoryReservation pressure(
                mfd::stream::detail::kStreamMemoryBudget - initialBytes - sharedBytes);
            for (int attempt = 0; attempt < 40; ++attempt) Tick(receiver, mfd::StreamPlayback::Play, 0, true);
            Require(receiver.Status().result == mfd::StreamResult::Unavailable && !receiver.Status().hasImage,
                    "Readback-only budget shortage became fatal");
            Require(mfd::stream::detail::ReservedStreamBytes() == mfd::stream::detail::kStreamMemoryBudget - sharedBytes,
                    "Failed readback construction leaked shared textures");
        }
        for (int attempt = 0; attempt < 300 && !receiver.Status().hasImage; ++attempt)
            Tick(receiver, mfd::StreamPlayback::Play, 0, true);
        const bool direct = receiver.Status().diagnostic.find("Direct GPU interop") != std::string::npos;
        if (mode.empty() && receiver.Status().hasImage && !direct)
        { std::printf("SKIP direct path: %s\n", receiver.Status().diagnostic.c_str()); result = 77; }
        else
        {
            std::printf("status: %s\n", receiver.Status().diagnostic.c_str());
            Require(receiver.Status().hasImage, "No frame within bounded startup deadline"); CheckPixels(receiver);
            Require(mode.empty() || mode == "runtime-lock" ? direct : receiver.Status().diagnostic.find("CPU fallback") != std::string::npos,
                    "Unexpected display path");
            if (mode == "runtime-lock")
            {
                for (int attempt = 0; attempt < 100 &&
                     receiver.Status().diagnostic.find("CPU fallback") == std::string::npos; ++attempt)
                    Tick(receiver, mfd::StreamPlayback::Play, 0, true);
                for (int attempt = 0; attempt < 100 && !receiver.Status().hasImage; ++attempt)
                    Tick(receiver, mfd::StreamPlayback::Play, 0, true);
                Require(receiver.Status().diagnostic.find("CPU fallback") != std::string::npos, "Runtime interop failure did not switch display path");
                CheckPixels(receiver);
            }
            unpack.Check();
            {
                mfd::stream::StreamReceiver duplicate(instance, "test");
                Require(duplicate.Status().result == mfd::StreamResult::Error, "Duplicate consumer accepted");
            }
            Tick(receiver, mfd::StreamPlayback::Play, 0, true);
            Require(receiver.Status().producerConnected, "Rejected duplicate invalidated the original endpoint");
            const auto pausedFrame = receiver.Status().consumedFrame;
            for (int attempt = 0; attempt < 60; ++attempt) Tick(receiver, mfd::StreamPlayback::Pause, 0, true);
            Require(receiver.Status().consumedFrame == pausedFrame, "Pause consumed new frames"); CheckPixels(receiver);
            for (int attempt = 0; attempt < 60; ++attempt) Tick(receiver, mfd::StreamPlayback::Play, 0, false);
            Require(receiver.Status().consumedFrame == pausedFrame, "Hidden stream consumed pixels");
            for (int attempt = 0; attempt < 30; ++attempt) Tick(receiver, mfd::StreamPlayback::Play, 0, true);
            Require(receiver.Status().consumedFrame > pausedFrame, "Producer starved during pause/hiding"); CheckPixels(receiver);
            Tick(receiver, mfd::StreamPlayback::Stop, 1, true);
            Require(receiver.Texture() == 0 && !receiver.Status().hasImage, "Stop retained a drawable image");
            const auto stoppedFrame = receiver.Status().consumedFrame;
            for (int attempt = 0; attempt < 40; ++attempt) Tick(receiver, mfd::StreamPlayback::Play, 1, true);
            Require(receiver.Status().consumedFrame > stoppedFrame, "Play after Stop failed"); CheckPixels(receiver);
            CheckStopWithBusyControl(receiver, instance);
            CheckStopBatch(receiver);
            producer.Kill();
            for (int attempt = 0; attempt < 10; ++attempt) Tick(receiver, mfd::StreamPlayback::Play, 2, true);
            Require(!receiver.Status().producerConnected && receiver.Status().hasImage, "Disconnect image/status mismatch");
            const auto generation = receiver.Status().generation;
            mfd::stream::detail::Endpoint observer;
            observer.Open(instance, "test", false);
            const auto previousConfiguration = ReadControl(observer);
            const auto bytesBeforeResize = mfd::stream::detail::ReservedStreamBytes();
            auto pressure = std::make_unique<mfd::stream::detail::MemoryReservation>(
                mfd::stream::detail::kStreamMemoryBudget - bytesBeforeResize);
            Child restarted(std::wstring(instance.begin(), instance.end()), 1);
            for (int attempt = 0; attempt < 300 && receiver.Status().result != mfd::StreamResult::Unavailable; ++attempt)
                Tick(receiver, mfd::StreamPlayback::Pause, 2, true);
            Require(receiver.Status().result == mfd::StreamResult::Unavailable, "Resize budget rejection became fatal");
            Require(receiver.Status().generation == generation && receiver.Width() == 64, "Budget rejection altered old generation");
            CheckUnchangedGeneration(previousConfiguration, ReadControl(observer));
            CheckPixels(receiver);
            Require(mfd::stream::detail::ReservedStreamBytes() == mfd::stream::detail::kStreamMemoryBudget,
                    "Rejected generations leaked reservations");
            pressure.reset();
            for (int attempt = 0; attempt < 40; ++attempt) Tick(receiver, mfd::StreamPlayback::Pause, 2, true);
            Require(receiver.Width() == 64, "Reconfiguration discarded paused image");
            for (int attempt = 0; attempt < 200 && receiver.Status().generation == generation; ++attempt)
                Tick(receiver, mfd::StreamPlayback::Play, 2, true);
            Require(receiver.Status().generation > generation, "Reconnect did not create a new resource generation");
            Require(receiver.Width() == 32 && receiver.Height() == 64, "Reconfiguration dimensions");
            CheckPixels(receiver);
            unpack.Check();
            Require(mfd::stream::detail::ReservedStreamBytes() == bytesBeforeResize,
                    "Old generation resources were not retired after resize");
            std::puts("PASS: cross-process pixels, playback, Stop/Play batch, hide, reconnect, budget recovery and retirement");
        }
    }
    catch (const std::exception& error) { std::printf("FAIL: %s\n", error.what()); result = 1; }
    if (mfd::stream::detail::ReservedStreamBytes() != initialBytes)
    { std::puts("FAIL: resource reservations leaked after receiver teardown"); result = 1; }
    if (childCleanupFailed) { std::puts("FAIL: producer teardown deadline expired"); result = 1; }
    SetEnvironmentVariableA("MFD_STREAM_TEST_INTEROP", nullptr);
    CloseTestContext(); return result;
}
}
int main(int argc, char** argv)
{
    try
    {
        if (argc == 4 && std::string(argv[1]) == "--producer") return Produce(argv[2], std::string(argv[3]) == "1");
        return Consume(argc == 3 && std::string(argv[1]) == "--mode" ? argv[2] : "");
    }
    catch (const std::exception& error) { std::puts(error.what()); return 1; }
}
