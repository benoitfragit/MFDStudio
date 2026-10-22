add_library(mfd_editor_widgets_api1 SHARED editor/src/WidgetEditorAbi.cpp)
target_compile_features(mfd_editor_widgets_api1 PRIVATE cxx_std_17)
target_compile_definitions(mfd_editor_widgets_api1 PRIVATE MFD_EDITOR_WIDGETS_EXPORTS)
target_include_directories(mfd_editor_widgets_api1 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/editor/include> $<INSTALL_INTERFACE:include>)
mfd_enable_target_warnings(mfd_editor_widgets_api1)
mfd_stage_runtime(mfd_editor_widgets_api1 WITH_RUNTIME_DLLS)
add_dependencies(mfd_editor mfd_editor_widgets_api1)
add_custom_command(TARGET mfd_editor_widgets_api1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:mfd_editor>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mfd_editor_widgets_api1> "$<TARGET_FILE_DIR:mfd_editor>/" VERBATIM)
install(TARGETS mfd_editor_widgets_api1
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_EDITOR_RUNTIME}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_EDITOR_RUNTIME}
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
install(DIRECTORY editor/include/ DESTINATION include COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
add_library(mfd_widgets_api1 SHARED runtime/src/WidgetRuntimeAbi.cpp)
target_compile_features(mfd_widgets_api1 PRIVATE cxx_std_17)
target_compile_definitions(mfd_widgets_api1 PRIVATE MFD_WIDGETS_EXPORTS)
target_include_directories(mfd_widgets_api1 PUBLIC $<BUILD_INTERFACE:${MFD_WIDGETS_INCLUDE_DIR}> $<INSTALL_INTERFACE:include>)
mfd_enable_target_warnings(mfd_widgets_api1)
mfd_stage_runtime(mfd_widgets_api1 WITH_RUNTIME_DLLS)
foreach(host IN ITEMS mfd_window mfd_editor)
    if(TARGET ${host})
        add_dependencies(${host} mfd_widgets_api1)
        add_custom_command(TARGET mfd_widgets_api1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${host}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mfd_widgets_api1> "$<TARGET_FILE_DIR:${host}>/" VERBATIM)
    endif()
endforeach()
install(TARGETS mfd_widgets_api1
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_API_RUNTIME})
install(DIRECTORY runtime/include/ DESTINATION include COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
install(DIRECTORY runtime/include/ DESTINATION include COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES generator/generate_widgets.py DESTINATION tools/extensions/apilevel1/widgets/generator COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
if(MFD_BUILD_TESTS)
    add_executable(mfd_widgets_tests tests/WidgetModelTests.cpp tests/WidgetCommandTests.cpp)
    mfd_configure_test_runtime_output(mfd_widgets_tests)
    target_link_libraries(mfd_widgets_tests PRIVATE mfd::client mfd_widgets_api1 EnTT::EnTT nlohmann_json::nlohmann_json GTest::gtest_main)
    target_compile_features(mfd_widgets_tests PRIVATE cxx_std_17)
    include(GoogleTest)
    if(WIN32)
        add_custom_command(TARGET mfd_widgets_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:mfd_api> $<TARGET_FILE:mfd_client_api> $<TARGET_FILE:mfd_widgets_api1>
                $<TARGET_FILE_DIR:mfd_widgets_tests>/ VERBATIM)
    endif()
    gtest_discover_tests(mfd_widgets_tests WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_widgets_tests>" DISCOVERY_TIMEOUT 60)
endif()

if(MFD_BUILD_TESTS)
    add_executable(mfd_widget_editor_tests tests/WidgetEditorTests.cpp
        editor/src/WidgetEditorModel.cpp editor/src/WidgetAssetService.cpp
        ${MFD_APILEVEL1_ROOT_DIR}/editor/src/ContainerEditorModel.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorDocumentSerializer.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorFileCommit.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorPageImportService.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorAssetReferenceIndexService.cpp)
    target_include_directories(mfd_widget_editor_tests PRIVATE editor/include ${MFD_APILEVEL1_EDITOR_INCLUDE_DIR} ${MFD_ROOT_DIR}/mfd_editor/include)
    target_link_libraries(mfd_widget_editor_tests PRIVATE mfd::api mfd_editor_widgets_api1 EnTT::EnTT nlohmann_json::nlohmann_json GTest::gtest_main)
    target_compile_features(mfd_widget_editor_tests PRIVATE cxx_std_17)
    mfd_configure_test_runtime_output(mfd_widget_editor_tests)
    if(WIN32)
        add_custom_command(TARGET mfd_widget_editor_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mfd_api> $<TARGET_FILE:mfd_editor_widgets_api1>
                $<TARGET_FILE_DIR:mfd_widget_editor_tests>/ VERBATIM)
    endif()
    gtest_discover_tests(mfd_widget_editor_tests WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_widget_editor_tests>" DISCOVERY_TIMEOUT 60)
endif()
