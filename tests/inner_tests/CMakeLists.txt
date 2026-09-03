set(INNER_TEST_NAME test_log_buffer)
set(INNER_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_log_buffer.cpp")

# redefine_file_macro("${INNER_TEST_SOURCE}")

add_executable("${INNER_TEST_NAME}" "${INNER_TEST_SOURCE}")

target_link_libraries("${INNER_TEST_NAME}" PRIVATE pans Threads::Threads)

target_include_directories("${INNER_TEST_NAME}" PRIVATE "${PROJECT_SOURCE_DIR}/pans/src")

set_target_properties("${INNER_TEST_NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PANS_TEST_OUTPUT_DIRECTORY}"
)

foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set_target_properties("${INNER_TEST_NAME}" PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${PANS_TEST_OUTPUT_DIRECTORY}"
    )
endforeach()
add_test(NAME "${INNER_TEST_NAME}" COMMAND "${INNER_TEST_NAME}")
