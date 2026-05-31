/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Dedicated libFuzzer entry point stressing JSON loading and validation.
 */

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "mfd/io/JsonLoader.h"

namespace
{
constexpr std::size_t kMaximumInputSize = 256U * 1024U;

class FuzzWorkspace
{
public:
    FuzzWorkspace()
    {
        const auto uniqueId = std::to_string(
            std::filesystem::file_time_type::clock::now().time_since_epoch().count());
        root_ = std::filesystem::temp_directory_path() / ("mfd_api_json_fuzzer_" + uniqueId);
        std::filesystem::create_directories(root_ / "pages");
        std::filesystem::create_directories(root_ / "reticles");
    }

    FuzzWorkspace(const FuzzWorkspace&) = delete;
    FuzzWorkspace& operator=(const FuzzWorkspace&) = delete;

    ~FuzzWorkspace()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] std::filesystem::path PageFile() const
    {
        return root_ / "pages" / "input.json";
    }

    [[nodiscard]] std::filesystem::path WindowFile() const
    {
        return root_ / "window.json";
    }

    void WritePageBytes(const std::uint8_t* data, const std::size_t size) const
    {
        std::ofstream output(PageFile(), std::ios::binary | std::ios::trunc);
        // std::ofstream::write consumes a byte buffer exposed as characters.
        output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }

    void WriteWrapperWindow() const
    {
        static constexpr std::string_view kWindowJson = R"json({
  "title": "fuzz_window",
  "width": 800,
  "height": 600,
  "reticleLibraryFolder": "reticles",
  "pages": [
    "pages/input.json"
  ]
})json";

        std::ofstream output(WindowFile(), std::ios::binary | std::ios::trunc);
        output.write(kWindowJson.data(), static_cast<std::streamsize>(kWindowJson.size()));
    }

private:
    std::filesystem::path root_;
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    if (data == nullptr || size > kMaximumInputSize)
    {
        return 0;
    }

    static FuzzWorkspace workspace;
    static mfd::JsonLoader loader;

    workspace.WritePageBytes(data, size);
    workspace.WriteWrapperWindow();

    try
    {
        loader.LoadDocument(workspace.PageFile());
    }
    catch (const std::exception&)
    {
    }

    try
    {
        loader.LoadWindowConfiguration(workspace.WindowFile());
    }
    catch (const std::exception&)
    {
    }

    return 0;
}
