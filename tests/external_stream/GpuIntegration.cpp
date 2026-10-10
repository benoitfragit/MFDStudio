/** @file @brief Bounded real-process D3D11/WGL integration test; excluded only from hosted GPU-less CI. */
#include "mfd/stream/StreamProducer.h"
#include "mfd/stream/StreamReceiver.h"
#include <windows.h>
#include <d3d11.h>
#include <GL/gl.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
void OpenTestContext();
void CloseTestContext();

namespace
{
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
        CloseHandle(process_.hThread); CloseHandle(process_.hProcess);
    }
    void Kill() { Require(TerminateProcess(process_.hProcess, 0) != FALSE, "Terminate producer"); }
private:
    PROCESS_INFORMATION process_ {};
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
    glBindTexture(GL_TEXTURE_2D, receiver.Texture());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const auto expected = receiver.Status().consumedFrame % 251;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4)
        Require(pixels[pixel] == expected && pixels[pixel + 1] == (pixel / 4) / receiver.Width() &&
                pixels[pixel + 2] == (pixel / 4) % receiver.Width() && pixels[pixel + 3] == 127,
                "Torn frame or invalid RGBA channels");
}
int Consume()
{
    OpenTestContext();
    int result = 0;
    try
    {
        const std::string instance = mfd::stream::NewStreamInstance();
        mfd::stream::StreamReceiver receiver(instance, "test");
        Child producer(std::wstring(instance.begin(), instance.end()));
        for (int attempt = 0; attempt < 300 && !receiver.Status().hasImage; ++attempt)
            Tick(receiver, mfd::StreamPlayback::Play, 0, true);
        if (receiver.Status().result == mfd::StreamResult::Unsupported)
        { std::printf("SKIP: %s\n", receiver.Status().diagnostic.c_str()); result = 77; }
        else
        {
            std::printf("status: %s\n", receiver.Status().diagnostic.c_str());
            Require(receiver.Status().hasImage, "No frame within bounded startup deadline"); CheckPixels(receiver);
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
            producer.Kill();
            for (int attempt = 0; attempt < 10; ++attempt) Tick(receiver, mfd::StreamPlayback::Play, 1, true);
            Require(!receiver.Status().producerConnected && receiver.Status().hasImage, "Disconnect image/status mismatch");
            const auto generation = receiver.Status().generation;
            Child restarted(std::wstring(instance.begin(), instance.end()), 1);
            for (int attempt = 0; attempt < 30; ++attempt) Tick(receiver, mfd::StreamPlayback::Pause, 1, true);
            Require(receiver.Width() == 64, "Reconfiguration discarded paused image");
            for (int attempt = 0; attempt < 200 && receiver.Status().generation == generation; ++attempt)
                Tick(receiver, mfd::StreamPlayback::Play, 1, true);
            Require(receiver.Status().generation > generation, "Reconnect did not create a new resource generation");
            Require(receiver.Width() == 32 && receiver.Height() == 64, "Reconfiguration dimensions");
            CheckPixels(receiver);
            std::puts("PASS: cross-process pixels, pause, stop, hide, resume, crash and reconnect");
        }
    }
    catch (const std::exception& error) { std::printf("FAIL: %s\n", error.what()); result = 1; }
    CloseTestContext(); return result;
}
}
int main(int argc, char** argv)
{
    try { return argc == 4 && std::string(argv[1]) == "--producer" ? Produce(argv[2], std::string(argv[3]) == "1") : Consume(); }
    catch (const std::exception& error) { std::puts(error.what()); return 1; }
}
