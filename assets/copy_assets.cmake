add_custom_target(copy_assets ALL)

function(xcopy_assets)
	set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
	set(ASSETS_OUT_DIR "${CMAKE_BINARY_DIR}/assets")
	file(GLOB_RECURSE ASSETS_FILES "${ASSETS_DIR}/*")
	foreach(ASSET_FILE ${ASSETS_FILES})
		get_filename_component(ASSET_FILE_NAME ${ASSET_FILE} NAME)
		add_custom_command(TARGET copy_assets POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${ASSET_FILE}
			${ASSETS_OUT_DIR}/${ASSET_FILE_NAME}
		)
	endforeach()
endfunction()

xcopy_assets()