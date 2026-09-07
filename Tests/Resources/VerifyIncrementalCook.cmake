if(NOT DEFINED ASSET_PIPELINE OR NOT DEFINED SOURCE_FILE OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "ASSET_PIPELINE, SOURCE_FILE and OUTPUT_DIR are required")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
    COMMAND "${ASSET_PIPELINE}" -pgltf --input-file "${SOURCE_FILE}" --output "${OUTPUT_DIR}" --force --meshlets --optimize
    RESULT_VARIABLE FIRST_RESULT
    OUTPUT_VARIABLE FIRST_OUTPUT
    ERROR_VARIABLE FIRST_ERROR
)
if(NOT FIRST_RESULT EQUAL 0)
    message(FATAL_ERROR "Initial cook failed (${FIRST_RESULT})\n${FIRST_OUTPUT}\n${FIRST_ERROR}")
endif()

set(GEOMETRY_FILE "${OUTPUT_DIR}/triangle.bin")
set(MANIFEST_FILE "${OUTPUT_DIR}/triangle.scene.json")
if(NOT EXISTS "${GEOMETRY_FILE}" OR NOT EXISTS "${MANIFEST_FILE}")
    message(FATAL_ERROR "Initial cook did not create the geometry and manifest outputs")
endif()
file(TIMESTAMP "${GEOMETRY_FILE}" FIRST_TIMESTAMP "%s")
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 2)

execute_process(
    COMMAND "${ASSET_PIPELINE}" -pgltf --input-file "${SOURCE_FILE}" --output "${OUTPUT_DIR}" --meshlets --optimize
    RESULT_VARIABLE SECOND_RESULT
    OUTPUT_VARIABLE SECOND_OUTPUT
    ERROR_VARIABLE SECOND_ERROR
)
if(NOT SECOND_RESULT EQUAL 0)
    message(FATAL_ERROR "Incremental cook failed (${SECOND_RESULT})\n${SECOND_OUTPUT}\n${SECOND_ERROR}")
endif()
file(TIMESTAMP "${GEOMETRY_FILE}" SECOND_TIMESTAMP "%s")

if(NOT FIRST_TIMESTAMP STREQUAL SECOND_TIMESTAMP)
    message(FATAL_ERROR "Unchanged input rewrote triangle.bin (${FIRST_TIMESTAMP} -> ${SECOND_TIMESTAMP})")
endif()
string(FIND "${SECOND_OUTPUT}${SECOND_ERROR}" "content hash unchanged" SKIP_MESSAGE_OFFSET)
if(SKIP_MESSAGE_OFFSET EQUAL -1)
    message(FATAL_ERROR "Incremental cook did not report a content-hash cache hit\n${SECOND_OUTPUT}\n${SECOND_ERROR}")
endif()
