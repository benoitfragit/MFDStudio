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
install(FILES generator/generate_widgets.py DESTINATION extensions/apilevel1/widgets/generator COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
if(MFD_BUILD_TESTS)
    add_executable(mfd_widgets_tests tests/WidgetModelTests.cpp tests/WidgetCommandTests.cpp)
    target_link_libraries(mfd_widgets_tests PRIVATE mfd::client mfd_widgets_api1 EnTT::EnTT nlohmann_json::nlohmann_json GTest::gtest_main)
    target_compile_features(mfd_widgets_tests PRIVATE cxx_std_17)
    include(GoogleTest)
    gtest_discover_tests(mfd_widgets_tests)
endif()
