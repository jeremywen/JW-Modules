#break up long string into shorter strings to work around MSVC string length constraints:
#https://learn.microsoft.com/en-us/cpp/c-language/maximum-string-length?redirectedfrom=MSDN&view=msvc-170

function(_chunkup INSTR OUTSTR)
	set (index 0)
	set (chunklen 500)

	string(LENGTH ${INSTR} len)
	set(chunked "")

	string(SUBSTRING ${INSTR} ${index} ${chunklen} chunk)
	string(APPEND chunked "R\"RNBOLIT(${chunk})RNBOLIT\"")
	math(EXPR index "${chunklen} + ${index}")
	while (index LESS ${len})
		string(SUBSTRING ${INSTR} ${index} ${chunklen} chunk)
		string(APPEND chunked "\nR\"RNBOLIT(${chunk})RNBOLIT\"")
		math(EXPR index "${chunklen} + ${index}")
	endwhile()

	set(${OUTSTR} ${chunked} PARENT_SCOPE)
endfunction()

function(rnbo_write_description_header DESCRIPTION_JSON OUTPUT_DIR)
	set(PRESETS_JSON "{}")

	#check for optional args
	set (optional_args ${ARGN})
	list(LENGTH optional_args optional_count)

	if (${optional_count} GREATER 0)
		list(GET optional_args 0 PRESETS_JSON)
	endif ()

	_chunkup(${DESCRIPTION_JSON} PATCHER_DESCRIPTION_JSON)
	_chunkup(${PRESETS_JSON} PATCHER_PRESETS_JSON)

	configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/rnbo_description.h.in ${OUTPUT_DIR}/rnbo_description.h)
endfunction()

function(rnbo_write_description_header_if_exists RNBO_DESCRIPTION_FILE DESCRIPTION_INCLUDE_DIR)

	if (EXISTS ${RNBO_DESCRIPTION_FILE})
		file(READ ${RNBO_DESCRIPTION_FILE} DESCRIPTION_JSON)

		#check for optional args
		set (optional_args ${ARGN})
		list(LENGTH optional_args optional_count)

		set(PRESETS_JSON "{}")
		if (${optional_count} GREATER 0)
			list(GET optional_args 0 PRESETS_JSON_FILE)
			if (EXISTS ${PRESETS_JSON_FILE})
				file(READ ${PRESETS_JSON_FILE} PRESETS_JSON)
			endif()
		endif ()

		rnbo_write_description_header("${DESCRIPTION_JSON}" ${DESCRIPTION_INCLUDE_DIR} "${PRESETS_JSON}")
		add_definitions(-DRNBO_INCLUDE_DESCRIPTION_FILE)
	endif()
endfunction()

