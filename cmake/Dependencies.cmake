# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(FetchContent)

function(mfd_locate_file out_var root pattern)
    file(GLOB_RECURSE matches CONFIGURE_DEPENDS "${root}/${pattern}")

    if(NOT matches)
        message(FATAL_ERROR "Unable to locate ${pattern} in ${root}")
    endif()

    list(GET matches 0 match)
    set(${out_var} "${match}" PARENT_SCOPE)
endfunction()

function(mfd_declare_dependency dependency_name)
    set(options)
    set(one_value_args GIT_REPOSITORY GIT_TAG LOCAL_ARCHIVE_NAME)
    cmake_parse_arguments(MFD_DEP "${options}" "${one_value_args}" "" ${ARGN})

    foreach(required_arg IN ITEMS GIT_REPOSITORY GIT_TAG LOCAL_ARCHIVE_NAME)
        if(NOT DEFINED MFD_DEP_${required_arg} OR MFD_DEP_${required_arg} STREQUAL "")
            message(FATAL_ERROR
                "mfd_declare_dependency(${dependency_name}) requires ${required_arg}.")
        endif()
    endforeach()

    if(NOT USE_LOCAL_PACKAGE)
        FetchContent_Declare(
            ${dependency_name}
            GIT_REPOSITORY ${MFD_DEP_GIT_REPOSITORY}
            GIT_TAG ${MFD_DEP_GIT_TAG})
        return()
    endif()

    set(mfd_local_archive
        "${MFD_LOCAL_PACKAGE_ROOT}/${dependency_name}/${MFD_DEP_LOCAL_ARCHIVE_NAME}")
    if(NOT EXISTS "${mfd_local_archive}")
        message(FATAL_ERROR
            "USE_LOCAL_PACKAGE is ON, but the local archive for '${dependency_name}' "
            "was not found at '${mfd_local_archive}'. "
            "Provide the source archive in MFD_LOCAL_PACKAGE_ROOT or reconfigure with "
            "-DUSE_LOCAL_PACKAGE=OFF.")
    endif()

    FetchContent_Declare(
        ${dependency_name}
        URL "${mfd_local_archive}")
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
    set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
    set(utf8_range_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(utf8_range_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
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

    # Local source archive when USE_LOCAL_PACKAGE is ON:
    # https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip
    # raylib intentionally stays static. The repository render layer is kept on
    # the host side (`mfd::render_raylib`) so `mfd_api` itself remains
    # render-agnostic and never needs to ship a separate raylib DLL.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    mfd_declare_dependency(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        LOCAL_ARCHIVE_NAME raylib-5.5.zip)
    FetchContent_MakeAvailable(raylib)
    if(mfd_had_build_shared_libs)
        set(BUILD_SHARED_LIBS "${mfd_previous_build_shared_libs}" CACHE BOOL "" FORCE)
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()

    # Local source archive when USE_LOCAL_PACKAGE is ON:
    # https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
    mfd_declare_dependency(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        LOCAL_ARCHIVE_NAME json-3.11.3.zip)
    FetchContent_MakeAvailable(nlohmann_json)

    # Local source archive when USE_LOCAL_PACKAGE is ON:
    # https://github.com/skypjack/entt/archive/refs/tags/v3.13.2.zip
    mfd_declare_dependency(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.2
        LOCAL_ARCHIVE_NAME entt-3.13.2.zip)
    FetchContent_MakeAvailable(entt)

    find_package(Protobuf CONFIG QUIET)
    if(NOT Protobuf_FOUND)
        # Local source archive when USE_LOCAL_PACKAGE is ON:
        # https://github.com/protocolbuffers/protobuf/archive/refs/tags/v29.4.zip
        #
        # The vendored protobuf tree still inspects BUILD_SHARED_LIBS for its
        # bundled abseil and utf8_range subprojects. Keep it OFF locally so the
        # runtime graph does not pull abseil_dll.dll or utf8_validity.dll into
        # the staged applications.
        mfd_declare_dependency(
            protobuf
            GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
            GIT_TAG v29.4
            LOCAL_ARCHIVE_NAME protobuf-29.4.zip)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(protobuf)
        if(mfd_had_build_shared_libs)
            set(BUILD_SHARED_LIBS "${mfd_previous_build_shared_libs}" CACHE BOOL "" FORCE)
        else()
            unset(BUILD_SHARED_LIBS CACHE)
        endif()
    endif()

    # Local source archive when USE_LOCAL_PACKAGE is ON:
    # https://github.com/ocornut/imgui/archive/refs/tags/v1.92.1.zip
    mfd_declare_dependency(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.1
        LOCAL_ARCHIVE_NAME imgui-1.92.1.zip)
    FetchContent_MakeAvailable(imgui)

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

    # Local source archive when USE_LOCAL_PACKAGE is ON:
    # https://github.com/raylib-extras/rlImGui/archive/286e11acd6c785004c9550c7ed3762add2ae3d47.zip
    # The pinned rlImGui input currently tracks one commit instead of a tagged release.
    mfd_declare_dependency(
        rlimgui
        GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
        # Pinned commit built against Dear ImGui 1.92.1 (matches the imgui dependency above).
        # Source: rlImGui README at this commit.
        GIT_TAG 286e11acd6c785004c9550c7ed3762add2ae3d47
        LOCAL_ARCHIVE_NAME rlImGui-286e11acd6c785004c9550c7ed3762add2ae3d47.zip)
    FetchContent_MakeAvailable(rlimgui)

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

        # Local source archive when USE_LOCAL_PACKAGE is ON:
        # https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
        mfd_declare_dependency(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            LOCAL_ARCHIVE_NAME googletest-1.15.2.zip)
        FetchContent_MakeAvailable(googletest)
    endif()
endfunction()
