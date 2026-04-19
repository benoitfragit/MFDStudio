/*
 * This file is part of MFDStudio.
 */
/**
 * @file
 * @brief Runtime loader for SHM adapter plugins.
 */

#include "mfd/plugin/MfdPluginLoader.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace mfd
{
class MfdPluginLoader::Impl
{
public:
#ifdef _WIN32
    HMODULE module = nullptr;
#else
    void* module = nullptr;
#endif
    std::unique_ptr<IMfdShmAdapterPlugin> plugin {};
};

MfdPluginLoader::~MfdPluginLoader()
{
    if (impl_ == nullptr)
    {
        return;
    }

#ifdef _WIN32
    if (impl_->module != nullptr)
    {
        ::FreeLibrary(impl_->module);
        impl_->module = nullptr;
    }
#else
    if (impl_->module != nullptr)
    {
        ::dlclose(impl_->module);
        impl_->module = nullptr;
    }
#endif
}

bool MfdPluginLoader::Load(const std::string& path, const std::string& symbol)
{
    impl_ = std::make_unique<Impl>();
#ifdef _WIN32
    impl_->module = ::LoadLibraryA(path.c_str());
    if (impl_->module == nullptr)
    {
        lastError_ = "LoadLibraryA failed for SHM plugin";
        impl_.reset();
        return false;
    }

    auto* factoryRaw = ::GetProcAddress(impl_->module, symbol.c_str());
#else
    impl_->module = ::dlopen(path.c_str(), RTLD_NOW);
    if (impl_->module == nullptr)
    {
        lastError_ = "dlopen failed for SHM plugin";
        impl_.reset();
        return false;
    }

    auto* factoryRaw = ::dlsym(impl_->module, symbol.c_str());
#endif

    if (factoryRaw == nullptr)
    {
        lastError_ = "Plugin factory symbol not found";
        impl_.reset();
        return false;
    }

    auto* factory = reinterpret_cast<MfdShmAdapterFactory>(factoryRaw);
    impl_->plugin.reset(factory());
    if (!impl_->plugin)
    {
        lastError_ = "Plugin factory returned null";
        impl_.reset();
        return false;
    }

    lastError_.clear();
    return true;
}

IMfdShmAdapterPlugin* MfdPluginLoader::Plugin() const noexcept
{
    return impl_ == nullptr ? nullptr : impl_->plugin.get();
}

std::string MfdPluginLoader::LastError() const
{
    return lastError_;
}
} // namespace mfd
