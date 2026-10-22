add_subdirectory(runtime)
add_subdirectory(editor)
add_subdirectory(widgets)

if(MFD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
