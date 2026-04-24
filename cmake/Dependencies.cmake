# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(FetchContent)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

function(mfd_locate_file out_var root pattern)
    file(GLOB_RECURSE matches CONFIGURE_DEPENDS "${root}/${pattern}")

    if(NOT matches)
        message(FATAL_ERROR "Unable to locate ${pattern} in ${root}")
    endif()

    list(GET matches 0 match)
    set(${out_var} "${match}" PARENT_SCOPE)
endfunction()

function(mfd_fetch_dependencies)
    # Keep local builds reproducible when the dependency sources are already
    # populated inside the build tree. This avoids an unnecessary network hop
    # on every CMake regenerate, which is especially helpful for offline or
    # sandboxed environments.
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    if(MSVC AND DEFINED MFD_MSVC_USE_DLL_RUNTIME AND NOT MFD_MSVC_USE_DLL_RUNTIME)
        set(protobuf_MSVC_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
    else()
        set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
    endif()

    set(mfd_had_build_shared_libs OFF)
    if(DEFINED BUILD_SHARED_LIBS)
        set(mfd_had_build_shared_libs ON)
        set(mfd_previous_build_shared_libs "${BUILD_SHARED_LIBS}")
    endif()

    # raylib must be shared so the EXE and the API DLL use the same rlgl/raylib state.
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5)
    FetchContent_MakeAvailable(raylib)
    if(mfd_had_build_shared_libs)
        set(BUILD_SHARED_LIBS "${mfd_previous_build_shared_libs}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()

    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)

    FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.2)
    FetchContent_MakeAvailable(entt)

    find_package(Protobuf CONFIG QUIET)
    if(NOT Protobuf_FOUND)
        FetchContent_Declare(
            protobuf
            GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
            GIT_TAG v29.4)
        FetchContent_MakeAvailable(protobuf)
    endif()

    set(imgui_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/imgui-src")
    if(NOT EXISTS "${imgui_SOURCE_DIR}/imgui.cpp")
        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.92.1)
        FetchContent_GetProperties(imgui)
        if(NOT imgui_POPULATED)
            FetchContent_Populate(imgui)
        endif()
    endif()

    if(NOT TARGET imgui)
        add_library(imgui STATIC
            "${imgui_SOURCE_DIR}/imgui.cpp"
            "${imgui_SOURCE_DIR}/imgui_demo.cpp"
            "${imgui_SOURCE_DIR}/imgui_draw.cpp"
            "${imgui_SOURCE_DIR}/imgui_tables.cpp"
            "${imgui_SOURCE_DIR}/imgui_widgets.cpp")

        target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")
        target_compile_features(imgui PUBLIC cxx_std_17)
    endif()

    set(rlimgui_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/rlimgui-src")
    if(NOT EXISTS "${rlimgui_SOURCE_DIR}")
        FetchContent_Declare(
            rlimgui
            GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
            # Pinned commit built against Dear ImGui 1.92.1 (matches the imgui dependency above).
            # Source: rlImGui README at this commit.
            GIT_TAG 286e11acd6c785004c9550c7ed3762add2ae3d47)
        FetchContent_GetProperties(rlimgui)
        if(NOT rlimgui_POPULATED)
            FetchContent_Populate(rlimgui)
        endif()
    endif()

    if(NOT TARGET rlimgui)
        mfd_locate_file(rlimgui_cpp "${rlimgui_SOURCE_DIR}" "rlImGui.cpp")
        get_filename_component(rlimgui_include_dir "${rlimgui_cpp}" DIRECTORY)

        add_library(rlimgui STATIC "${rlimgui_cpp}")
        target_include_directories(rlimgui PUBLIC "${rlimgui_include_dir}")
        target_link_libraries(rlimgui PUBLIC raylib imgui)
        target_compile_features(rlimgui PUBLIC cxx_std_17)
    endif()

    if(MFD_BUILD_TESTS)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        if(MSVC AND DEFINED MFD_MSVC_USE_DLL_RUNTIME AND NOT MFD_MSVC_USE_DLL_RUNTIME)
            set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
        else()
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        endif()

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2)
        FetchContent_MakeAvailable(googletest)
    endif()
endfunction()
