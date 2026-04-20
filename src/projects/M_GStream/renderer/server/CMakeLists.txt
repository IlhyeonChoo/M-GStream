project(M_GStream_server)

set(SERVER_SOURCES
    CameraPoseAdapter.cpp
    CameraPoseAdapter.hpp
    Config.hpp
    JpegEncoder.cpp
    JpegEncoder.hpp
    MjpegStreamer.cpp
    MjpegStreamer.hpp
    RemoteStreamServer.cpp
    RemoteStreamServer.hpp
    ServerProtocol.cpp
    ServerProtocol.hpp
)

find_path(
    BOOST_BEAST_INCLUDE_DIR
    NAMES boost/beast/http.hpp
    HINTS ${Boost_INCLUDE_DIR} ${Boost_INCLUDE_DIRS}
)
if(NOT BOOST_BEAST_INCLUDE_DIR)
    message(FATAL_ERROR "SIBR_BUILD_REMOTE_STREAM requires Boost.Beast headers.")
endif()

add_library(${PROJECT_NAME} STATIC ${SERVER_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${OpenCV_LIBRARIES}
    sibr_assets
    sibr_graphics
    sibr_system
)
target_compile_definitions(${PROJECT_NAME} PUBLIC
    SIBR_EXTENDED_GAUSSIAN_SERVER_STATIC_DEFINE
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SIBR_EXTENDED_GAUSSIAN_REMOTE_WWW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/www"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER "projects/M_GStream/renderer/server"
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "M_GStream_server JSON backend: picojson")
message(STATUS "M_GStream_server HTTP/WebSocket backend: Boost.Beast")

if(TARGET TurboJPEG::TurboJPEG)
    target_link_libraries(${PROJECT_NAME} PRIVATE TurboJPEG::TurboJPEG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIBR_EXTENDED_GAUSSIAN_TURBOJPEG_AVAILABLE=1)
    message(STATUS "M_GStream_server JPEG backend: TurboJPEG enabled")
else()
    message(WARNING "M_GStream_server JPEG backend: TurboJPEG not found. Falling back to OpenCV JPEG encoding for M5.")
endif()
