# Exercise the actual delivery components, including dynamically loaded modules.
foreach(required IN ITEMS BUILD_DIRECTORY BUILD_CONFIG SOURCE_DIRECTORY TEST_ROOT PYTHON_EXECUTABLE GENERATOR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing installation test parameter: ${required}")
    endif()
endforeach()

function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Command failed (${result}): ${ARGV}\n${output}\n${error}")
    endif()
endfunction()
function(expect_file path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Installed file is missing: ${path}")
    endif()
endfunction()

foreach(component IN ITEMS mfd_window_launcher_runtime mfd_editor_runtime mfd_client_api_development)
    set(prefix "${TEST_ROOT}/${component}")
    file(REMOVE_RECURSE "${prefix}")
    run_checked("${CMAKE_COMMAND}" --install "${BUILD_DIRECTORY}" --config "${BUILD_CONFIG}"
        --component "${component}" --prefix "${prefix}")
endforeach()
foreach(component IN ITEMS mfd_window_launcher_runtime mfd_editor_runtime)
    expect_file("${TEST_ROOT}/${component}/mfd_containers_api1.dll")
    expect_file("${TEST_ROOT}/${component}/mfd_widgets_api1.dll")
endforeach()
expect_file("${TEST_ROOT}/mfd_editor_runtime/mfd_editor_containers_api1.dll")
expect_file("${TEST_ROOT}/mfd_editor_runtime/mfd_editor_widgets_api1.dll")
if(EXISTS "${TEST_ROOT}/mfd_window_launcher_runtime/mfd_editor_widgets_api1.dll")
    message(FATAL_ERROR "The Window package must not depend on the widget editor module.")
endif()

set(sdk "${TEST_ROOT}/mfd_client_api_development")
foreach(schema IN ITEMS mfd_commands mfd_feedback mfd_containers mfd_widgets)
    expect_file("${sdk}/share/mfd/proto/${schema}.proto")
endforeach()
expect_file("${sdk}/include/mfd/client/Widget.h")
expect_file("${sdk}/include/mfd/apilevel1/widgets/WidgetCommands.h")
set(consumer "${TEST_ROOT}/consumer")
file(REMOVE_RECURSE "${consumer}")
file(MAKE_DIRECTORY "${consumer}")
run_checked("${PYTHON_EXECUTABLE}" "${sdk}/tools/generator/scripts/generate_ui.py"
    --window-json "${SOURCE_DIRECTORY}/examples/apilevel1/widgets/assets/windows/widgets_window.json"
    --output-header "${consumer}/WidgetsExampleUi.h" --output-source "${consumer}/WidgetsExampleUi.cpp"
    --output-map "${consumer}/widgets.generated.map" --namespace widgets_example
    --ui-class-name WidgetsExampleUi --header-include WidgetsExampleUi.h)
file(WRITE "${consumer}/main.cpp" "#include \"WidgetsExampleUi.h\"\nint main() { widgets_example::WidgetsExampleUi ui; return ui.BuildBatch().empty() ? 0 : 1; }\n")
file(WRITE "${consumer}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\nproject(InstalledWidgetConsumer LANGUAGES CXX)\n"
    "set(CMAKE_MSVC_RUNTIME_LIBRARY \"${MSVC_RUNTIME}\")\n"
    "find_package(MFDStudioClientApi CONFIG REQUIRED PATHS \"${sdk}/cmake\" NO_DEFAULT_PATH)\n"
    "add_executable(widget_consumer main.cpp WidgetsExampleUi.cpp)\n"
    "target_link_libraries(widget_consumer PRIVATE MFDStudio::ClientApi)\n")
set(configure "${CMAKE_COMMAND}" -S "${consumer}" -B "${consumer}/build" -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
    list(APPEND configure -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
    list(APPEND configure -T "${GENERATOR_TOOLSET}")
endif()
run_checked(${configure})
run_checked("${CMAKE_COMMAND}" --build "${consumer}/build" --config "${BUILD_CONFIG}")
