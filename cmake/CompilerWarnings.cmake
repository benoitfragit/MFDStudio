# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

function(mfd_enable_target_warnings target_name)
    if(NOT MFD_ENABLE_WARNINGS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(mfd_warning_options
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough)

        if(MSVC)
            list(APPEND mfd_warning_options /EHsc)
        endif()
    elseif(MSVC)
        set(mfd_warning_options
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /EHsc
            /sdl
            /utf-8
            /external:anglebrackets
            /external:W0
            /w14242
            /w14254
            /w14263
            /w14265
            /w14287
            /we4289
            /w14296
            /w14311
            /w14545
            /w14546
            /w14547
            /w14549
            /w14555
            /w14619
            /w14640
            /w14826
            /w14905
            /w14906
            /w14928
            /wd4251
            /wd4275)
    else()
        set(mfd_warning_options
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough)
    endif()

    if(MFD_ENABLE_WARNINGS_AS_ERRORS)
        if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            list(APPEND mfd_warning_options /WX)
        else()
            list(APPEND mfd_warning_options -Werror)
        endif()
    endif()

    target_compile_options(${target_name}
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:${mfd_warning_options}>)
endfunction()
