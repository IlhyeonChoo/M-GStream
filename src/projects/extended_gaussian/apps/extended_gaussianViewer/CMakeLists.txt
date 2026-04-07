project(extended_gaussianViewer_app)

file(GLOB SOURCES "*.cpp" "*.h" "*.hpp")
source_group("Source Files" FILES ${SOURCES})

file(GLOB RESOURCES "resources/*.ini")
source_group("Resources Files" FILES ${RESOURCES})

set(WINDOWS_PORTABLE_SCRIPT "${CMAKE_SOURCE_DIR}/tools/windows/run_installed_viewer.cmd")
set(WINDOWS_PORTABLE_DOC "${CMAKE_SOURCE_DIR}/docs/extended_gaussian_windows_portable_bundle_ko.md")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME}

	${Boost_LIBRARIES}
	${ASSIMP_LIBRARIES}
	${GLEW_LIBRARIES}
	${OPENGL_LIBRARIES}
	${OpenCV_LIBRARIES}
	OpenMP::OpenMP_CXX
	extended_gaussian
	sibr_view
	sibr_assets
	sibr_renderer
)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "projects/extended_gaussian/apps")

if (WIN32)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
		COMMAND_EXPAND_LISTS
	)

	add_custom_target(${PROJECT_NAME}_extended_gaussian_sync
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:extended_gaussian>
			$<TARGET_FILE_DIR:${PROJECT_NAME}>
		DEPENDS extended_gaussian
	)
	add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_extended_gaussian_sync)
	set_target_properties(${PROJECT_NAME}_extended_gaussian_sync PROPERTIES FOLDER "projects/extended_gaussian/apps")
endif()

## High level macro to install in an homogen way all our ibr targets
include(install_runtime)
ibr_install_target(${PROJECT_NAME}
    INSTALL_PDB                         ## mean install also MSVC IDE *.pdb file (DEST according to target type)
	RESOURCES  	${RESOURCES}
	SCRIPTS     ${WINDOWS_PORTABLE_SCRIPT}
	RSC_FOLDER 	"extended_gaussian"
    STANDALONE  ${INSTALL_STANDALONE}   ## mean call install_runtime with bundle dependencies resolution
    COMPONENT   ${PROJECT_NAME}_install ## will create custom target to install only this project
)

if (WIN32)
	install(FILES ${WINDOWS_PORTABLE_DOC}
		DESTINATION docs
		COMPONENT ${PROJECT_NAME}_install
	)
endif()
