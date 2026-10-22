add_library(mfd_editor_containers_api1 SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/apilevel1/ContainerEditorAbi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ContainerEditorExtension.cpp)

target_compile_features(mfd_editor_containers_api1 PUBLIC cxx_std_17)
target_compile_definitions(mfd_editor_containers_api1 PRIVATE MFD_EDITOR_CONTAINERS_API1_EXPORTS)
target_include_directories(mfd_editor_containers_api1
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
mfd_enable_target_warnings(mfd_editor_containers_api1)
set_target_properties(mfd_editor_containers_api1 PROPERTIES OUTPUT_NAME "mfd_editor_containers_api1")

mfd_stage_runtime(mfd_editor_containers_api1 WITH_RUNTIME_DLLS)

add_dependencies(mfd_editor mfd_editor_containers_api1 mfd_containers_api1)
add_custom_command(
    TARGET mfd_editor_containers_api1
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:mfd_editor>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:mfd_editor_containers_api1>
            $<TARGET_FILE_DIR:mfd_editor>/
    VERBATIM)

install(TARGETS mfd_editor_containers_api1
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_EDITOR_RUNTIME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
