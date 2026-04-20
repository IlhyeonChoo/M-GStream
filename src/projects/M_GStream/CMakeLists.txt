project(M_GStream_all)

if(WIN32)
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for M_GStream" OFF)
else()
    option(SIBR_BUILD_REMOTE_STREAM "Build remote stream support targets for M_GStream" ON)
endif()
message(STATUS "M_GStream remote stream modules: ${SIBR_BUILD_REMOTE_STREAM}")

add_subdirectory(renderer)
add_subdirectory(apps)

include(install_runtime)
subdirectory_target(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR} "projects/M_GStream")
