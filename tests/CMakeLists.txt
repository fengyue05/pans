file(GLOB PANS_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

list(SORT PANS_TEST_SOURCES)

find_package(Threads REQUIRED)

set(PANS_TEST_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests")

foreach(TEST_SOURCE IN LISTS PANS_TEST_SOURCES)
    get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)

    if(TARGET "${TEST_NAME}")
        message(FATAL_ERROR "Cannot create test target '${TEST_NAME}': a target with that name already exists")
    endif()

    add_executable("${TEST_NAME}" "${TEST_SOURCE}")

    # pans 的 PUBLIC 依赖会让测试同时继承头文件路径和 pans_options。
    target_link_libraries("${TEST_NAME}" PRIVATE pans Threads::Threads)

    set_target_properties("${TEST_NAME}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PANS_TEST_OUTPUT_DIRECTORY}"
    )

    # 多配置生成器默认会追加 Debug/Release 子目录，这里保持输出路径一致。
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)
        set_target_properties("${TEST_NAME}" PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${PANS_TEST_OUTPUT_DIRECTORY}"
        )
    endforeach()

    add_test(NAME "${TEST_NAME}" COMMAND "${TEST_NAME}")
endforeach()

# add_subdirectory(inner_tests)

