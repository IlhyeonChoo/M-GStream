project(extended_gaussian_all)

if(WIN32)
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for extended_gaussian" OFF)
else()
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for extended_gaussian" ON)
endif()
message(STATUS "extended_gaussian remote stream modules: ${SIBR_BUILD_REMOTE_STREAM}")

add_subdirectory(apps)
add_subdirectory(renderer)

include(install_runtime)
subdirectory_target(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR} "projects/extended_gaussian")
