set(MFD_CONTAINERS_RUNTIME_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/apilevel1/ContainerCommands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/apilevel1/ContainerInputRouter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/apilevel1/ContainerLayoutEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/apilevel1/ContainerRuntimeAbi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/apilevel1/ContainerRuntimeHost.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/apilevel1/ContainerTypes.h)

set(MFD_CONTAINERS_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ContainerInputRouter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ContainerLayoutEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ContainerRuntimeAbi.cpp)

add_library(mfd_containers_api1 SHARED
    ${MFD_CONTAINERS_RUNTIME_HEADERS}
    ${MFD_CONTAINERS_RUNTIME_SOURCES})

target_compile_features(mfd_containers_api1 PUBLIC cxx_std_17)
target_compile_definitions(mfd_containers_api1 PRIVATE MFD_CONTAINERS_API1_EXPORTS)
target_include_directories(mfd_containers_api1
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(mfd_containers_api1 PRIVATE mfd::model)
mfd_enable_target_warnings(mfd_containers_api1)

set_target_properties(mfd_containers_api1 PROPERTIES OUTPUT_NAME "mfd_containers_api1")

mfd_stage_runtime(mfd_containers_api1 WITH_RUNTIME_DLLS)

if(TARGET mfd_window)
    add_dependencies(mfd_window mfd_containers_api1)
    add_custom_command(
        TARGET mfd_containers_api1 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:mfd_containers_api1>
                $<TARGET_FILE_DIR:mfd_window>/
        VERBATIM)
endif()

install(TARGETS mfd_containers_api1
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_API_RUNTIME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
