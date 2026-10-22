#pragma once
#include "mfd/apilevel1/widgets/WidgetRuntimeAbi.h"
#include <filesystem>
#include <memory>
#include <string>
namespace mfd::api1
{
class WidgetRuntimeHost final
{
public:
    WidgetRuntimeHost();
    ~WidgetRuntimeHost();
    WidgetRuntimeHost(WidgetRuntimeHost&&) noexcept;
    WidgetRuntimeHost& operator=(WidgetRuntimeHost&&) noexcept;
    WidgetRuntimeHost(const WidgetRuntimeHost&) = delete;
    WidgetRuntimeHost& operator=(const WidgetRuntimeHost&) = delete;
    bool LoadForProcess(const std::filesystem::path& windowFile, std::string& error);
    bool Load(const std::filesystem::path& libraryFile, std::string& error);
    bool IsLoaded() const noexcept;
    bool Evaluate(const MfdWidgetInputV1& input, MfdWidgetOutputV1& output) const noexcept;
    void Unload() noexcept;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
