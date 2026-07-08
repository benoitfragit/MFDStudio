/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Export/import macro for the reusable `hud_runtime` shared library.
 */

#if defined(_WIN32) || defined(__CYGWIN__)
#    if defined(HUD_RUNTIME_BUILDING_DLL)
        /** @brief Marks a public symbol exported by `hud_runtime`. */
#        define HUD_RUNTIME_API __declspec(dllexport)
#    else
        /** @brief Marks a public symbol imported from `hud_runtime`. */
#        define HUD_RUNTIME_API __declspec(dllimport)
#    endif
#else
    /** @brief Marks a public symbol when no DLL decoration is required. */
#    define HUD_RUNTIME_API
#endif
