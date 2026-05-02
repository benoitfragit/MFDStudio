/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Legacy decoration macro kept across the public MFD headers.
 */

#if defined(_WIN32) || defined(__CYGWIN__)
#    if defined(MFD_API_EXPORTS)
        /** @brief Marks a public symbol exported by one MFD component DLL. */
#        define MFD_API __declspec(dllexport)
#    elif defined(MFD_API_DLL)
        /** @brief Marks a public symbol imported from one MFD component DLL. */
#        define MFD_API __declspec(dllimport)
#    else
        /** @brief Marks a public symbol when no DLL decoration is required. */
#        define MFD_API
#    endif
#else
    /** @brief Marks a public symbol when no DLL decoration is required. */
#    define MFD_API
#endif
