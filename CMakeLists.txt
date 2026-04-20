cmake_minimum_required(VERSION 3.24)

if(EXISTS "/usr/bin/gcc-12")
    set(CMAKE_C_COMPILER "/usr/bin/gcc-12" CACHE FILEPATH "Preferred C compiler" FORCE)
endif()
if(EXISTS "/usr/bin/g++-12")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++-12" CACHE FILEPATH "Preferred C++ compiler" FORCE)
    set(CMAKE_CUDA_HOST_COMPILER "/usr/bin/g++-12" CACHE FILEPATH "Preferred CUDA host compiler" FORCE)
endif()
if(EXISTS "/usr/local/cuda-12.8/bin/nvcc")
    set(CMAKE_CUDA_COMPILER "/usr/local/cuda-12.8/bin/nvcc" CACHE FILEPATH "Preferred CUDA compiler" FORCE)
    set(CUDAToolkit_ROOT "/usr/local/cuda-12.8" CACHE PATH "Preferred CUDA toolkit root" FORCE)
endif()
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type for single-config generators" FORCE)
endif()

project(extended_gaussian_split LANGUAGES C CXX)

include(CheckCXXCompilerFlag)
get_filename_component(currentBuildTool "${CMAKE_BUILD_TOOL}" NAME_WE)
if(NOT WIN32)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    if(currentBuildTool MATCHES "(make|gmake|ninja)")
        string(APPEND CMAKE_C_FLAGS " -fPIC -Wall -Wno-unknown-pragmas -Wno-sign-compare -g")
        string(APPEND CMAKE_CXX_FLAGS " -fpermissive -fPIC -Wall -Wno-unknown-pragmas -Wno-sign-compare -g -fms-extensions")
    endif()
endif()

include(FetchContent)

if(NOT WIN32 AND NOT TARGET nativefiledialog)
    add_library(nativefiledialog STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/local_third_party/nativefiledialog_stub/nfd_stub.cpp"
    )
    target_include_directories(nativefiledialog PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/local_third_party/nativefiledialog_stub"
    )
    set_target_properties(nativefiledialog PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        FOLDER "extlibs"
    )
endif()

set(SIBR_FORK_URL
    "git@github.com:IlhyeonChoo/sibr_core.git"
    CACHE STRING "Pinned SIBR fork URL"
)
set(SIBR_PINNED_COMMIT
    "29b3cfcb186148fe6037a1d0204e9a1bfb0c3eaf"
    CACHE STRING "Pinned SIBR fork commit"
)

message(STATUS "Fetching SIBR fork from ${SIBR_FORK_URL} @ ${SIBR_PINNED_COMMIT}")

set(BUILD_DOCUMENTATION OFF CACHE BOOL "Disable SIBR documentation in the split repo" FORCE)
set(BUILD_SIBR ON CACHE BOOL "Build SIBR core libraries required by extended_gaussian" FORCE)
set(BUILD_IBR_BASIC OFF CACHE BOOL "" FORCE)
set(BUILD_IBR_DATASET_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_IBR_GAUSSIANVIEWER OFF CACHE BOOL "" FORCE)
set(BUILD_IBR_REMOTE OFF CACHE BOOL "" FORCE)
set(BUILD_IBR_ULR OFF CACHE BOOL "" FORCE)

if(NOT WIN32)
    find_path(SYSTEM_EIGEN3_INCLUDE_DIR
        NAMES Eigen/Core
        PATHS /usr/include/eigen3 /usr/local/include/eigen3 /opt/local/include/eigen3
    )
    find_path(SYSTEM_ASSIMP_INCLUDE_DIR
        NAMES assimp/config.h
        PATHS /usr /usr/local /opt/local
        PATH_SUFFIXES include
    )
    find_library(SYSTEM_ASSIMP_LIBRARY
        NAMES assimp
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /opt/local/lib
    )
    if(SYSTEM_ASSIMP_INCLUDE_DIR AND SYSTEM_ASSIMP_LIBRARY)
        set(ASSIMP_INCLUDE_DIR "${SYSTEM_ASSIMP_INCLUDE_DIR}" CACHE PATH "System ASSIMP include directory" FORCE)
        set(ASSIMP_LIBRARY "${SYSTEM_ASSIMP_LIBRARY}" CACHE FILEPATH "System ASSIMP library" FORCE)
        set(ASSIMP_LIBRARIES "${SYSTEM_ASSIMP_LIBRARY}" CACHE STRING "System ASSIMP libraries" FORCE)
    endif()

    find_library(SYSTEM_GLFW_LIBRARY
        NAMES glfw glfw3
        PATHS /lib /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /opt/local/lib
    )
    if(SYSTEM_GLFW_LIBRARY)
        set(GLFW_LIBRARY "${SYSTEM_GLFW_LIBRARY}" CACHE FILEPATH "System GLFW library" FORCE)
    endif()
endif()

if(NOT WIN32 AND SYSTEM_EIGEN3_INCLUDE_DIR)
    include_directories("${SYSTEM_EIGEN3_INCLUDE_DIR}")
    add_definitions(-DEIGEN_INITIALIZE_MATRICES_BY_ZERO)
endif()

if(NOT WIN32 AND SYSTEM_GLFW_LIBRARY AND NOT TARGET glfw3)
    add_library(glfw3 SHARED IMPORTED GLOBAL)
    set_target_properties(glfw3 PROPERTIES
        IMPORTED_LOCATION "${SYSTEM_GLFW_LIBRARY}"
    )
endif()

FetchContent_Declare(
    sibr_core
    GIT_REPOSITORY "${SIBR_FORK_URL}"
    GIT_TAG "${SIBR_PINNED_COMMIT}"
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(sibr_core)

# Keep install artifacts for split-repo targets under this repository instead
# of the fetched dependency checkout.
set(CMAKE_INSTALL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/install")
set(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_ROOT}")

if(DEFINED CMAKE_BUILD_TYPE)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(CMAKE_DEBUG_POSTFIX "_d")
    elseif(CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
        set(CMAKE_RELWITHDEBINFO_POSTFIX "_rwdi")
    elseif(CMAKE_BUILD_TYPE MATCHES "MinSizeRel")
        set(CMAKE_MINSIZEREL_POSTFIX "_msr")
    elseif(CMAKE_BUILD_TYPE MATCHES "Release")
        set(CMAKE_RELEASE_POSTFIX "")
    endif()

    set(CMAKE_INSTALL_PREFIX_${CMAKE_BUILD_TYPE} "${CMAKE_INSTALL_ROOT}")
    set(CMAKE_OUTPUT_LIB_${CMAKE_BUILD_TYPE} "${CMAKE_INSTALL_PREFIX_${CMAKE_BUILD_TYPE}}/lib")
    set(CMAKE_OUTPUT_BIN_${CMAKE_BUILD_TYPE} "${CMAKE_INSTALL_PREFIX_${CMAKE_BUILD_TYPE}}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CMAKE_BUILD_TYPE} "${CMAKE_OUTPUT_LIB_${CMAKE_BUILD_TYPE}}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CMAKE_BUILD_TYPE} "${CMAKE_OUTPUT_LIB_${CMAKE_BUILD_TYPE}}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CMAKE_BUILD_TYPE} "${CMAKE_OUTPUT_BIN_${CMAKE_BUILD_TYPE}}")
    set(CMAKE_PDB_OUTPUT_DIRECTORY_${CMAKE_BUILD_TYPE} "${CMAKE_OUTPUT_BIN_${CMAKE_BUILD_TYPE}}")
endif()

if(NOT WIN32)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

list(APPEND CMAKE_MODULE_PATH
    "${sibr_core_SOURCE_DIR}/cmake"
    "${sibr_core_SOURCE_DIR}/cmake/Modules"
)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(SIBR_SPLIT_CORE_SOURCE_DIR "${sibr_core_SOURCE_DIR}/src")
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${SIBR_SPLIT_CORE_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/extlibs/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/extlibs/picojson"
    "${CMAKE_CURRENT_SOURCE_DIR}/extlibs/rapidxml"
    "${CMAKE_CURRENT_SOURCE_DIR}/extlibs/nanoflann"
    "${CMAKE_CURRENT_SOURCE_DIR}/extlibs/mrf"
)

if(NOT TARGET OpenMP::OpenMP_CXX AND DEFINED OpenMP_CXX_FLAGS)
    add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)

    separate_arguments(OPENMP_CXX_FLAG_LIST UNIX_COMMAND "${OpenMP_CXX_FLAGS}")
    set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
        INTERFACE_COMPILE_OPTIONS "${OPENMP_CXX_FLAG_LIST}"
    )

    set(OPENMP_CXX_LINK_ITEMS "${OPENMP_CXX_FLAG_LIST}")
    foreach(OPENMP_CXX_LIB_NAME ${OpenMP_CXX_LIB_NAMES})
        if(DEFINED OpenMP_${OPENMP_CXX_LIB_NAME}_LIBRARY)
            list(APPEND OPENMP_CXX_LINK_ITEMS "${OpenMP_${OPENMP_CXX_LIB_NAME}_LIBRARY}")
        endif()
    endforeach()
    set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
        INTERFACE_LINK_LIBRARIES "${OPENMP_CXX_LINK_ITEMS}"
    )
endif()
if(NOT TARGET OpenMP::OpenMP_CXX)
    message(FATAL_ERROR "OpenMP::OpenMP_CXX target is required for extended_gaussianViewer_app.")
endif()

add_subdirectory(src/projects/extended_gaussian)
