project(extended_gaussian_server)

set(SERVER_SOURCES
    CameraPoseAdapter.cpp
    CameraPoseAdapter.hpp
    Config.hpp
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
    sibr_assets
    sibr_system
)
target_compile_definitions(${PROJECT_NAME} PUBLIC
    SIBR_EXTENDED_GAUSSIAN_SERVER_STATIC_DEFINE
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER "projects/extended_gaussian/renderer/server"
    POSITION_INDEPENDENT_CODE ON
)

message(STATUS "extended_gaussian_server JSON backend: picojson")
message(STATUS "extended_gaussian_server HTTP/WebSocket backend: Boost.Beast")

find_path(TURBOJPEG_INCLUDE_DIR NAMES turbojpeg.h)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg libturbojpeg)
if(TURBOJPEG_INCLUDE_DIR AND TURBOJPEG_LIBRARY)
    message(STATUS "extended_gaussian_server JPEG backend: TurboJPEG found (${TURBOJPEG_LIBRARY})")
else()
    message(WARNING "extended_gaussian_server JPEG backend: TurboJPEG not found. M5 will require TurboJPEG for MJPEG encoding.")
endif()
