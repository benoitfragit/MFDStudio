if(NOT WIN32)
    return()
endif()
set(stream_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
client_api_generate_ui(
    WINDOW_JSON "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows/external_stream_window.json"
    OUTPUT_HEADER "${stream_generated}/ExternalStreamUi.h"
    OUTPUT_SOURCE "${stream_generated}/ExternalStreamUi.cpp"
    OUTPUT_MAP "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows/external_stream_window.generated.map"
    NAMESPACE "external_stream_demo"
    UI_CLASS_NAME "ExternalStreamUi"
    HEADER_INCLUDE "ExternalStreamUi.h")
add_library(mfd_stream_demo SHARED native/DemoProducer.cpp native/DemoControl.cpp native/exports.def
    "${stream_generated}/ExternalStreamUi.cpp")
target_compile_features(mfd_stream_demo PRIVATE cxx_std_17)
target_compile_definitions(mfd_stream_demo PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(mfd_stream_demo PRIVATE "${stream_generated}")
target_link_libraries(mfd_stream_demo PRIVATE mfd::external_stream mfd::client)
mfd_enable_target_warnings(mfd_stream_demo)
set(stream_scripts "${CMAKE_CURRENT_BINARY_DIR}/scripts")
file(MAKE_DIRECTORY "${stream_scripts}")
foreach(script_name producer.py pattern.py)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${script_name}" "${stream_scripts}/${script_name}" COPYONLY)
endforeach()
foreach(script_name Start-ExternalStream.bat Start-MfdWindow.bat)
    configure_file("${MFD_ROOT_DIR}/Scripts/${script_name}" "${stream_scripts}/${script_name}" COPYONLY)
endforeach()
mfd_stage_runtime(mfd_stream_demo WITH_RUNTIME_DLLS
    ROOT_ASSET_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    SCRIPT_SOURCE_DIR "${stream_scripts}"
    SCRIPT_NAMES Start-ExternalStream.bat Start-MfdWindow.bat producer.py pattern.py)
add_custom_command(TARGET mfd_stream_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/producer.py" "$<TARGET_FILE_DIR:mfd_stream_demo>/producer.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/pattern.py" "$<TARGET_FILE_DIR:mfd_stream_demo>/pattern.py")
