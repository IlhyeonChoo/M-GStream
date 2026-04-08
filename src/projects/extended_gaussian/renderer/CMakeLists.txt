set(CNU_PROJECT "extended_gaussian")
project(${CNU_PROJECT} LANGUAGES CXX CUDA)

set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)
set(EXTENDED_GAUSSIAN_CUDA_ARCHITECTURES
    "75-real;86-real;89-real;120"
    CACHE STRING "CUDA architectures for extended_gaussian and bundled CudaRasterizer builds. Covers Turing (SM 7.5) through Blackwell (SM 12.0)."
)
set(EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES "${EXTENDED_GAUSSIAN_CUDA_ARCHITECTURES}")

# Keep a project-local portable default instead of relying on generator-specific
# 'native' detection or stale cache values from older build trees.
set(CMAKE_CUDA_ARCHITECTURES "${EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES}")
message(STATUS "extended_gaussian CUDA architectures: ${EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES}")

sibr_gitlibrary(TARGET CudaRasterizer
    GIT_REPOSITORY  "https://github.com/graphdeco-inria/diff-gaussian-rasterization.git"
    GIT_TAG         "255c532d5fd0c3d83afec0e8e49574b65ae381bd"
)

find_package(CUDAToolkit REQUIRED)

file(GLOB_RECURSE ALL_FILES 
    "*.cpp" "*.h" "*.hpp"  "cuda/*.cu" "cuda/*.cuh"
    "shaders/*.frag" "shaders/*.vert" "shaders/*.geom"
)

file(GLOB SHADERS "shaders/*.frag" "shaders/*.vert" "shaders/*.geom")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${ALL_FILES})

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

if(TARGET CudaRasterizer)
    set_target_properties(CudaRasterizer PROPERTIES
        CUDA_ARCHITECTURES "${EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES}"
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

add_definitions( -DSIBR_EXTENDED_GAUSSIAN_EXPORTS -DBOOST_ALL_DYN_LINK  )

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "projects/${CNU_PROJECT}/renderer")
set_target_properties(${PROJECT_NAME} PROPERTIES 
    CUDA_ARCHITECTURES "${EXTENDED_GAUSSIAN_EFFECTIVE_CUDA_ARCHITECTURES}"
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_STANDARD 17
)

## High level macro to install
include(install_runtime)
ibr_install_target(${PROJECT_NAME}
    INSTALL_PDB
    SHADERS ${SHADERS}
    RSC_FOLDER ${CNU_PROJECT}
    COMPONENT   ${PROJECT_NAME}_install
)
