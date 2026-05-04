set(CHU_PROJECT "M_GStream")
set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)
set(MGSTREAM_CUDA_ARCHITECTURES
    "75-real;86-real;89-real;120"
    CACHE STRING "CUDA architectures for M_GStream and bundled CudaRasterizer builds. Covers Turing (SM 7.5) through Blackwell (SM 12.0)."
)
set(MGSTREAM_EFFECTIVE_CUDA_ARCHITECTURES "${MGSTREAM_CUDA_ARCHITECTURES}")

# Keep a project-local portable default instead of relying on generator-specific
# 'native' detection or stale cache values from older build trees.
set(CMAKE_CUDA_ARCHITECTURES "${MGSTREAM_EFFECTIVE_CUDA_ARCHITECTURES}")
message(STATUS "M_GStream CUDA architectures: ${MGSTREAM_EFFECTIVE_CUDA_ARCHITECTURES}")

project(${CHU_PROJECT} LANGUAGES CXX CUDA)

sibr_gitlibrary(TARGET CudaRasterizer
    GIT_REPOSITORY  "https://github.com/graphdeco-inria/diff-gaussian-rasterization.git"
    GIT_TAG         "255c532d5fd0c3d83afec0e8e49574b65ae381bd"
)

if(TARGET CudaRasterizer)
    # CUDA 12.8 does not expose fixed-width integer types transitively through
    # the upstream rasterizer's include chain.
    target_compile_options(CudaRasterizer PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--pre-include=cstdint>
    )
endif()

find_package(CUDAToolkit REQUIRED)

file(GLOB_RECURSE ALL_FILES 
    "*.cpp" "*.h" "*.hpp"  "cuda/*.cu" "cuda/*.cuh"
    "shaders/*.frag" "shaders/*.vert" "shaders/*.geom"
)
file(GLOB_RECURSE SERVER_FILES "server/*.cpp" "server/*.h" "server/*.hpp")
list(REMOVE_ITEM ALL_FILES ${SERVER_FILES})

file(GLOB SHADERS "shaders/*.frag" "shaders/*.vert" "shaders/*.geom")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${ALL_FILES})

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(TARGET CudaRasterizer)
    set_target_properties(CudaRasterizer PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CUDA_ARCHITECTURES "${MGSTREAM_EFFECTIVE_CUDA_ARCHITECTURES}"
    )
endif()

if(MSVC)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--compiler-options=/Zc:preprocessor>
)
endif()

include_directories(${Boost_INCLUDE_DIRS} .)
if (WIN32)
target_link_libraries(${PROJECT_NAME}
	${Boost_LIBRARIES}
	${ASSIMP_LIBRARIES}
	${GLEW_LIBRARIES}
	${OPENGL_LIBRARIES}
	${OpenCV_LIBRARIES}
	glfw3
	sibr_system
	sibr_view
	sibr_assets
	sibr_renderer
	CUDA::cudart
	CudaRasterizer
)
else()
target_link_libraries(${PROJECT_NAME}
	${Boost_LIBRARIES}
	${ASSIMP_LIBRARIES}
	${GLEW_LIBRARIES}
	${OPENGL_LIBRARIES}
	${OpenCV_LIBRARIES}
	glfw
	sibr_system
	sibr_view
	sibr_assets
	sibr_renderer
	CUDA::cudart
	CudaRasterizer
)
endif()

add_definitions( -DSIBR_MGSTREAM_EXPORTS -DBOOST_ALL_DYN_LINK  )

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "projects/${CHU_PROJECT}/renderer")
set_target_properties(${PROJECT_NAME} PROPERTIES 
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_STANDARD 17
)

if(SIBR_BUILD_REMOTE_STREAM)
    add_subdirectory(server)
endif()

## High level macro to install
include(install_runtime)
set(M_GSTREAM_INSTALL_PDB_FLAG "")
if(MSVC)
    set(M_GSTREAM_INSTALL_PDB_FLAG INSTALL_PDB)
endif()
ibr_install_target(${PROJECT_NAME}
    ${M_GSTREAM_INSTALL_PDB_FLAG}
    SHADERS ${SHADERS}
    RSC_FOLDER ${CHU_PROJECT}
    COMPONENT   ${PROJECT_NAME}_install
)
