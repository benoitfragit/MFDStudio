/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Export/import macros used by the public client animation API.
 */

#if defined(_WIN32) || defined(__CYGWIN__)
#    if defined(MFD_CLIENT_API_EXPORTS)
        /** @brief Marks a public symbol exported by `client_api`. */
#        define MFD_CLIENT_API __declspec(dllexport)
#    elif defined(MFD_CLIENT_API_DLL)
        /** @brief Marks a public symbol imported from `client_api`. */
#        define MFD_CLIENT_API __declspec(dllimport)
#    else
        /** @brief Marks a public symbol when no DLL decoration is required. */
#        define MFD_CLIENT_API
#    endif
#else
    /** @brief Marks a public symbol when no DLL decoration is required. */
#    define MFD_CLIENT_API
#endif
