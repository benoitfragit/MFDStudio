# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_MOCKUP_MINIMAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MockupUi.cpp)

set(MFD_MOCKUP_MINIMAL_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MockupUi.h)

add_executable(mfd_mockup_minimal ${MFD_MOCKUP_MINIMAL_HEADERS} ${MFD_MOCKUP_MINIMAL_SOURCES})

target_link_libraries(mfd_mockup_minimal PRIVATE mfd::api)

set_target_properties(mfd_mockup_minimal PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_mockup_minimal>")

add_custom_command(
    TARGET mfd_mockup_minimal
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:mfd_mockup_minimal>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:mfd_mockup_minimal>
        "$<TARGET_FILE_DIR:mfd_mockup_minimal>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(mfd_mockup_minimal WITH_ASSETS WITH_RUNTIME_DLLS)
