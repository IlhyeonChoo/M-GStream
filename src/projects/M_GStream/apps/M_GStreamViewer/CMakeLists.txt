project(M_GStreamViewer_app)

file(GLOB SOURCES "*.cpp" "*.h" "*.hpp")
source_group("Source Files" FILES ${SOURCES})

file(GLOB RESOURCES "resources/*.ini")
source_group("Resources Files" FILES ${RESOURCES})

set(WINDOWS_PORTABLE_SCRIPT "")
set(WINDOWS_PORTABLE_CHECK_SCRIPT "")
set(WINDOWS_PORTABLE_DOC "")
if(WIN32)
	set(WINDOWS_PORTABLE_SCRIPT "${CMAKE_SOURCE_DIR}/tools/windows/run_installed_viewer.cmd")
	set(WINDOWS_PORTABLE_CHECK_SCRIPT "${CMAKE_SOURCE_DIR}/tools/windows/check_windows_runtime.ps1")
	set(WINDOWS_PORTABLE_DOC "${CMAKE_SOURCE_DIR}/docs/M_GStream_windows_portable_bundle_ko.md")
endif()
set(WINDOWS_PORTABLE_EXTRA_RUNTIME_FILES "")
set(WINDOWS_PORTABLE_EXTRA_RUNTIME_IMPORTED_TARGETS "")
set(WINDOWS_PORTABLE_EXTRA_RUNTIME_TARGETS "")
set(LINUX_REMOTE_STREAM_RUNTIME_DIRS "")

function(append_runtime_target_file_expr target_name files_var)
	if(NOT TARGET ${target_name})
		return()
	endif()

	set(local_files "${${files_var}}")
	list(APPEND local_files "$<TARGET_FILE:${target_name}>")
	list(REMOVE_DUPLICATES local_files)
	set(${files_var} "${local_files}" PARENT_SCOPE)
endfunction()

if(WIN32)
	if(TARGET CUDA::cudart)
		list(APPEND WINDOWS_PORTABLE_EXTRA_RUNTIME_IMPORTED_TARGETS CUDA::cudart)
		append_runtime_target_file_expr(CUDA::cudart WINDOWS_PORTABLE_EXTRA_RUNTIME_FILES)
	endif()

	if(TARGET xatlas)
		list(APPEND WINDOWS_PORTABLE_EXTRA_RUNTIME_TARGETS xatlas)
		append_runtime_target_file_expr(xatlas WINDOWS_PORTABLE_EXTRA_RUNTIME_FILES)
	endif()
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME}
	${Boost_LIBRARIES}
	${ASSIMP_LIBRARIES}
	${GLEW_LIBRARIES}
	${OPENGL_LIBRARIES}
	${OpenCV_LIBRARIES}
	OpenMP::OpenMP_CXX
	M_GStream
	sibr_view
	sibr_assets
	sibr_renderer
)
if(NOT WIN32)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib"
	)
endif()
if(TARGET M_GStream_server)
	target_link_libraries(${PROJECT_NAME} M_GStream_server)
	target_compile_definitions(${PROJECT_NAME} PRIVATE SIBR_MGSTREAM_REMOTE_STREAM_BUILD=1)
	if(SIBR_TURBOJPEG_LIBRARY_DIR)
		list(APPEND LINUX_REMOTE_STREAM_RUNTIME_DIRS ${SIBR_TURBOJPEG_LIBRARY_DIR})
	endif()
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "projects/M_GStream/apps")

if (WIN32)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
		COMMAND_EXPAND_LISTS
	)

	if(WINDOWS_PORTABLE_EXTRA_RUNTIME_FILES)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				${WINDOWS_PORTABLE_EXTRA_RUNTIME_FILES}
				$<TARGET_FILE_DIR:${PROJECT_NAME}>
			COMMAND_EXPAND_LISTS
		)
	endif()

	add_custom_target(${PROJECT_NAME}_M_GStream_sync
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:M_GStream>
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
		DEPENDS M_GStream
	)
	add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_M_GStream_sync)
	set_target_properties(${PROJECT_NAME}_M_GStream_sync PROPERTIES FOLDER "projects/M_GStream/apps")
endif()

if(WIN32)
	if(WINDOWS_PORTABLE_EXTRA_RUNTIME_IMPORTED_TARGETS)
		install(IMPORTED_RUNTIME_ARTIFACTS ${WINDOWS_PORTABLE_EXTRA_RUNTIME_IMPORTED_TARGETS}
			DESTINATION bin
			COMPONENT ${PROJECT_NAME}_install
		)
	endif()

	if(WINDOWS_PORTABLE_EXTRA_RUNTIME_TARGETS)
		install(TARGETS ${WINDOWS_PORTABLE_EXTRA_RUNTIME_TARGETS}
			RUNTIME DESTINATION bin
			COMPONENT ${PROJECT_NAME}_install
		)
	endif()
endif()

## High level macro to install in an homogen way all our ibr targets
include(install_runtime)
set(M_GSTREAM_INSTALL_PDB_FLAG "")
if(MSVC)
	set(M_GSTREAM_INSTALL_PDB_FLAG INSTALL_PDB)
endif()
if(NOT WIN32)
	if(DEFINED CMAKE_BUILD_TYPE)
		resourceFile(ADD "shaders/core" "resources/core"
			CONFIG_TYPE ${CMAKE_BUILD_TYPE}
			FILE_PATH "${CMAKE_INSTALL_PREFIX_${CMAKE_BUILD_TYPE}}/ibr_resources.ini"
		)
	endif()
	foreach(CONFIG_TYPES ${CMAKE_CONFIGURATION_TYPES})
		string(TOUPPER ${CONFIG_TYPES} CONFIG_TYPES_UC)
		resourceFile(ADD "shaders/core" "resources/core"
			CONFIG_TYPE ${CONFIG_TYPES}
			FILE_PATH "${CMAKE_INSTALL_PREFIX_${CONFIG_TYPES_UC}}/ibr_resources.ini"
		)
	endforeach()
endif()
ibr_install_target(${PROJECT_NAME}
    ${M_GSTREAM_INSTALL_PDB_FLAG}       ## mean install also MSVC IDE *.pdb file (DEST according to target type)
	RESOURCES  	${RESOURCES}
	SCRIPTS     ${WINDOWS_PORTABLE_SCRIPT} ${WINDOWS_PORTABLE_CHECK_SCRIPT}
	RSC_FOLDER 	"M_GStream"
	DIRS        ${LINUX_REMOTE_STREAM_RUNTIME_DIRS}
    STANDALONE  ${INSTALL_STANDALONE}   ## mean call install_runtime with bundle dependencies resolution
	COMPONENT   ${PROJECT_NAME}_install ## will create custom target to install only this project
)

if(NOT WIN32)
	if(DEFINED sibr_core_SOURCE_DIR)
		install(DIRECTORY "${sibr_core_SOURCE_DIR}/install/lib/"
			DESTINATION lib
			COMPONENT ${PROJECT_NAME}_install
			OPTIONAL
			FILES_MATCHING PATTERN "libsibr_*.so*"
		)
		install(DIRECTORY "${sibr_core_SOURCE_DIR}/install/shaders/core/"
			DESTINATION shaders/core
			COMPONENT ${PROJECT_NAME}_install
			OPTIONAL
		)
		install(DIRECTORY "${sibr_core_SOURCE_DIR}/install/resources/core/"
			DESTINATION resources/core
			COMPONENT ${PROJECT_NAME}_install
			OPTIONAL
		)
	endif()

	install(DIRECTORY "${CMAKE_SOURCE_DIR}/extlibs/xatlas/build/"
		DESTINATION lib
		COMPONENT ${PROJECT_NAME}_install
		OPTIONAL
		FILES_MATCHING PATTERN "libxatlas.so*"
	)
	install(DIRECTORY "${CMAKE_SOURCE_DIR}/extlibs/mrf/build/"
		DESTINATION lib
		COMPONENT ${PROJECT_NAME}_install
		OPTIONAL
		FILES_MATCHING PATTERN "libmrf.so*"
	)
endif()

if(TARGET M_GStream_server)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/projects/M_GStream/renderer/server/www/
		DESTINATION resources/M_GStream/server/www
		COMPONENT ${PROJECT_NAME}_install
	)

	install(FILES
		${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so
		${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0
		${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0.2.0
		DESTINATION bin
		COMPONENT ${PROJECT_NAME}_install
		OPTIONAL
	)
	install(FILES
		${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so
		${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0
		${SIBR_TURBOJPEG_LIBRARY_DIR}/libturbojpeg.so.0.2.0
		DESTINATION lib
		COMPONENT ${PROJECT_NAME}_install
		OPTIONAL
	)
endif()

if (WIN32)
	install(FILES ${WINDOWS_PORTABLE_DOC}
		DESTINATION docs
		COMPONENT ${PROJECT_NAME}_install
	)
endif()
