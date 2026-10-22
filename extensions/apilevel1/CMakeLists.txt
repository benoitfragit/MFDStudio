add_subdirectory(runtime)
add_subdirectory(editor)
add_subdirectory(widgets)

# These modules are loaded by name, so TARGET_RUNTIME_DLLS cannot discover them.
foreach(host_component IN ITEMS ${MFD_COMPONENT_WINDOW_LAUNCHER_RUNTIME} ${MFD_COMPONENT_EDITOR_RUNTIME})
    install(FILES "$<TARGET_FILE:mfd_containers_api1>" "$<TARGET_FILE:mfd_widgets_api1>"
        DESTINATION . COMPONENT ${host_component})
endforeach()

if(MFD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
