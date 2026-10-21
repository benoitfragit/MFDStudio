add_subdirectory(runtime)
add_subdirectory(editor)

if(MFD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
