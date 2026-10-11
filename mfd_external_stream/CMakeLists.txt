add_library(mfd_external_stream STATIC
    src/StreamProtocol.cpp
    src/NativeStream.cpp
    src/StreamProducer.cpp
    src/StreamReceiver.cpp
    src/StreamBudget.cpp
    src/StreamGl.cpp
    src/StreamReadback.cpp
    src/StreamGeneration.cpp)
add_library(mfd::external_stream ALIAS mfd_external_stream)
target_compile_features(mfd_external_stream PUBLIC cxx_std_17)
target_include_directories(mfd_external_stream PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${MFD_COMMON_API_PUBLIC_INCLUDE_DIR})
target_include_directories(mfd_external_stream PRIVATE src)
if(WIN32)
    target_compile_definitions(mfd_external_stream PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(mfd_external_stream PUBLIC d3d11 dxgi opengl32)
endif()
mfd_enable_target_warnings(mfd_external_stream)
