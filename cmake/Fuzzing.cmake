# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

if(MFD_ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "MFD_ENABLE_FUZZING requires a Clang-based compiler. "
            "Use the dedicated external LLVM/libFuzzer build.")
    endif()

    if(MSVC AND NOT MFD_MSVC_RUNTIME STREQUAL "static")
        message(FATAL_ERROR
            "MFD_ENABLE_FUZZING requires the static MSVC runtime on Windows. "
            "Configure the dedicated fuzzing build with -DMFD_MSVC_RUNTIME=static.")
    endif()
endif()

function(mfd_enable_fuzzing_instrumentation target_name)
    if(NOT MFD_ENABLE_FUZZING)
        return()
    endif()

    if(WIN32)
        target_compile_definitions(${target_name}
            PRIVATE
                _DISABLE_VECTOR_ANNOTATION
                _DISABLE_STRING_ANNOTATION)
    endif()

    target_compile_options(${target_name}
        PRIVATE
            -fsanitize=fuzzer-no-link,address
            -fno-omit-frame-pointer)
endfunction()

function(mfd_enable_fuzzing target_name)
    if(NOT MFD_ENABLE_FUZZING)
        return()
    endif()

    mfd_enable_fuzzing_instrumentation(${target_name})

    target_link_options(${target_name}
        PRIVATE
            -fsanitize=fuzzer,address)
endfunction()
