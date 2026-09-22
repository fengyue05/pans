set(PANS_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party" CACHE PATH "third-party source")
set(PANS_THIRD_PARTY_OUTPUT_DIR "${PANS_THIRD_PARTY_DIR}/output" CACHE PATH "compiled third-party output")
set(PANS_THIRD_PARTY_BUILD_TYPE Release CACHE STRING "third-party build-type")
set_property(CACHE PANS_THIRD_PARTY_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

option(PANS_UPDATE_SUBMODULES "Initialize missing third-party git submodules" ON)
option(PANS_REUSE_INSTALLED_DEPENDENCIES "Reuse compatible dependencies from third_party/output" ON)

# 子模块哨兵文件
set(_pans_required_submodules
    "third_party/yaml-cpp|CMakeLists.txt"
    "third_party/abseil-cpp|CMakeLists.txt"
    "third_party/protobuf|CMakeLists.txt"
    "third_party/rapidjson|include/rapidjson/document.h"
    "third_party/tinyxml2|CMakeLists.txt"
    "third_party/kcp|ikcp.c"
    "third_party/zlib|CMakeLists.txt"
)

find_package(Git QUIET)

function(pans_init_third_party_submodules)
    set(missing_submodules)
    foreach(entry IN LISTS _pans_required_submodules)
        string(REPLACE "|" ";" parts "${entry}")
        list(GET parts 0 submodule_path)
        list(GET parts 1 sentinel)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${submodule_path}/${sentinel}")
            list(APPEND missing_submodules "${submodule_path}")
        endif()
    endforeach()

    if(NOT missing_submodules)
        return()
    endif()
    
    if(NOT PANS_UPDATE_SUBMODULES)
        message(FATAL_ERROR
            "Missing third-party source trees: ${missing_submodules}\n"
            "Enable PANS_UPDATE_SUBMODULES or run:\n"
            "  git submodule update --init --recursive")
    endif()
    
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/.git")
        message(FATAL_ERROR
            "Missing third-party source trees: ${missing_submodules}\n"
            "The source tree is not a git checkout, so CMake cannot initialize them.")
    endif()
    
    if(NOT Git_FOUND)
        message(FATAL_ERROR "Git is required to initialize missing submodules.")
    endif()
    
    message(STATUS "Initializing missing git submodules: ${missing_submodules}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive -- ${missing_submodules}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        RESULT_VARIABLE submodule_result
        OUTPUT_VARIABLE submodule_output
        ERROR_VARIABLE submodule_error
    )
    
    if(NOT submodule_result EQUAL 0)
        message(FATAL_ERROR
            "git submodule update failed (exit=${submodule_result}):\n"
            "${submodule_output}${submodule_error}")
    endif()
    
    foreach(entry IN LISTS _pans_required_submodules)
        string(REPLACE "|" ";" parts "${entry}")
        list(GET parts 0 submodule_path)
        list(GET parts 1 sentinel)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${submodule_path}/${sentinel}")
            message(FATAL_ERROR
                "Required third-party source is still missing: "
                "${submodule_path}/${sentinel}")
        endif()
    endforeach()
endfunction()

pans_init_third_party_submodules()

# 获取子模块commit ID的函数
function(pans_get_source_revision source_dir output_variable)
    if(Git_FOUND AND EXISTS "${source_dir}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" rev-parse HEAD
            RESULT_VARIABLE revision_result
            OUTPUT_VARIABLE revision
            ERROR_VARIABLE revision_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT revision_result EQUAL 0)
            message(FATAL_ERROR "Failed to read third-party revision in ${source_dir}: ${revision_error}")
        endif()
    else()
        file(SHA256 "${source_dir}/CMakeLists.txt" revision)
    endif()
    set(${output_variable} "${revision}" PARENT_SCOPE)
endfunction()

function(pans_add_cmake_dependency)
    # 解析函数的命名参数
    cmake_parse_arguments(PARSE_ARGV 0 dep ""
        "NAME;SOURCE_DIR;SOURCE_REVISION"
        "CMAKE_ARGS;SENTINELS;DEPENDS;KEY_MATERIAL")
    
    if(NOT dep_NAME OR NOT dep_SOURCE_DIR OR NOT dep_SOURCE_REVISION)
        message(FATAL_ERROR "pans_add_cmake_dependency requires NAME, SOURCE_DIR and SOURCE_REVISION")
    endif()
    
    if(NOT dep_SENTINELS)
        message(FATAL_ERROR "pans_add_cmake_dependency(${dep_NAME}) requires SENTINELS")
    endif()
    
    string(CONCAT cache_material
        "${dep_SOURCE_REVISION};${CMAKE_C_COMPILER};${CMAKE_C_COMPILER_ID};"
        "${CMAKE_C_COMPILER_VERSION};${CMAKE_CXX_COMPILER};${CMAKE_CXX_COMPILER_ID};"
        "${CMAKE_CXX_COMPILER_VERSION};${CMAKE_SYSTEM_NAME};${CMAKE_SYSTEM_PROCESSOR};"
        "${CMAKE_GENERATOR};${CMAKE_GENERATOR_PLATFORM};${CMAKE_GENERATOR_TOOLSET};"
        "${CMAKE_OSX_ARCHITECTURES};${CMAKE_OSX_DEPLOYMENT_TARGET};"
        "${CMAKE_MSVC_RUNTIME_LIBRARY};${PANS_THIRD_PARTY_BUILD_TYPE};static;"
        "${dep_CMAKE_ARGS};${dep_KEY_MATERIAL}")

    # 任何配置变化都会触发新的编译，保证不会误用前一个结果，从而保证第三方库的ABI一致
    string(SHA256 cache_key "${cache_material}")
    string(SUBSTRING "${cache_key}" 0 16 cache_key_short)
    
    set(install_dir "${PANS_THIRD_PARTY_OUTPUT_DIR}/${dep_NAME}/${cache_key_short}")
    set(build_dir "${PANS_THIRD_PARTY_OUTPUT_DIR}/build/${dep_NAME}-${cache_key_short}")
    set(cache_stamp "${install_dir}/.pans-cache-${cache_key}")

    set(installed_outputs "${cache_stamp}")
    set(cache_complete TRUE)
    foreach(sentinel IN LISTS dep_SENTINELS)
        if(IS_ABSOLUTE "${sentinel}")
            set(sentinel_path "${sentinel}")
        else()
            set(sentinel_path "${install_dir}/${sentinel}")
        endif()
        list(APPEND installed_outputs "${sentinel_path}")
        if(NOT EXISTS "${sentinel_path}")
            set(cache_complete FALSE)
        endif()
    endforeach()

    if(NOT EXISTS "${cache_stamp}")
        set(cache_complete FALSE)
    endif()

    set(configure_command "${CMAKE_COMMAND}" -S "${dep_SOURCE_DIR}" -B "${build_dir}" -G "${CMAKE_GENERATOR}")
    # 然后把当前项目使用的目标平台和编译工具集传递给第三方库，保持一致
    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND configure_command -A "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND configure_command -T "${CMAKE_GENERATOR_TOOLSET}")
    endif()
    
    list(APPEND configure_command
        "-DCMAKE_INSTALL_PREFIX=${install_dir}"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_BUILD_TYPE=${PANS_THIRD_PARTY_BUILD_TYPE}"
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
        "-DBUILD_TESTING=OFF"
    )
    
    if(CMAKE_C_COMPILER)
        list(APPEND configure_command "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
    endif()
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif()
    if(CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND configure_command "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()
    if(CMAKE_OSX_ARCHITECTURES)
        list(APPEND configure_command "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
    endif()
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND configure_command "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    list(APPEND configure_command ${dep_CMAKE_ARGS})
    # 把完整构建流程保存到build_commands裏面，一共有四步, 分别是cmake, 编译，安装，写入缓存标记
    set(build_commands
        COMMAND ${configure_command}
        COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --config "${PANS_THIRD_PARTY_BUILD_TYPE}" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL}"
        COMMAND "${CMAKE_COMMAND}" --install "${build_dir}" --config "${PANS_THIRD_PARTY_BUILD_TYPE}"
        COMMAND "${CMAKE_COMMAND}" "-DPANS_DEPS_STAMP_FILE=${cache_stamp}" "-DPANS_DEPS_STAMP_VALUE=${cache_key}" -P "${PROJECT_SOURCE_DIR}/cmake/writeDependencyStamp.cmake"
    )

    set(ready_target "pans_dep_${dep_NAME}_ready")
    string(REPLACE "-" "_" ready_target "${ready_target}")
    
    if(PANS_REUSE_INSTALLED_DEPENDENCIES)
        add_custom_command(
            OUTPUT ${installed_outputs}
            ${build_commands}
            COMMENT "Building and installing third-party dependency: ${dep_NAME}"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        add_custom_target("${ready_target}" DEPENDS ${installed_outputs})
    else()
        add_custom_target("${ready_target}"
            ${build_commands}
            COMMENT "Rebuilding third-party dependency: ${dep_NAME}"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
    endif()
    # 添加第三方库的依赖，用于建立第三方库之间的构建顺序
    if(dep_DEPENDS)
        add_dependencies("${ready_target}" ${dep_DEPENDS})
    endif()
    
    if(cache_complete AND PANS_REUSE_INSTALLED_DEPENDENCIES)
        message(STATUS "${dep_NAME}: reusing ${install_dir}")
    else()
        message(STATUS "${dep_NAME}: build scheduled; output ${install_dir}")
    endif()
    
    string(TOUPPER "${dep_NAME}" variable_prefix)
    string(REPLACE "-" "_" variable_prefix "${variable_prefix}")
    set("PANS_${variable_prefix}_INSTALL_DIR" "${install_dir}" PARENT_SCOPE)
    set("PANS_${variable_prefix}_CACHE_KEY" "${cache_key}" PARENT_SCOPE)
    set("PANS_${variable_prefix}_CACHE_COMPLETE" "${cache_complete}" PARENT_SCOPE)
    set("PANS_${variable_prefix}_DEPENDENCY_TARGET" "${ready_target}" PARENT_SCOPE)
endfunction()

# 统一的静态库文件名模板
set(_pans_static_library "${CMAKE_STATIC_LIBRARY_PREFIX}@NAME@${CMAKE_STATIC_LIBRARY_SUFFIX}")

# -------- abseil-cpp --------
set(_pans_absl_source "${PANS_THIRD_PARTY_DIR}/abseil-cpp")
pans_get_source_revision("${_pans_absl_source}" _pans_absl_revision)
string(REPLACE "@NAME@" "absl_base" _pans_absl_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME abseil-cpp
    SOURCE_DIR "${_pans_absl_source}"
    SOURCE_REVISION "${_pans_absl_revision}"
    CMAKE_ARGS
        "-DBUILD_SHARED_LIBS=OFF"
        "-DABSL_ENABLE_INSTALL=ON"
        "-DABSL_PROPAGATE_CXX_STD=ON"
        "-DABSL_BUILD_TESTING=OFF"
    SENTINELS
        "lib/cmake/absl/abslConfig.cmake"
        "include/absl/base/config.h"
        "lib/${_pans_absl_library}"
)

# -------- zlib --------
set(_pans_zlib_source "${PANS_THIRD_PARTY_DIR}/zlib")
pans_get_source_revision("${_pans_zlib_source}" _pans_zlib_revision)
string(REPLACE "@NAME@" "z" _pans_zlib_library "${_pans_static_library}")
if(MSVC)
    string(REPLACE "@NAME@" "zlibstatic" _pans_zlib_library "${_pans_static_library}")
endif()
pans_add_cmake_dependency(
    NAME zlib
    SOURCE_DIR "${_pans_zlib_source}"
    SOURCE_REVISION "${_pans_zlib_revision}"
    CMAKE_ARGS
        "-DZLIB_BUILD_TESTING=OFF"
        "-DZLIB_BUILD_SHARED=OFF"
        "-DZLIB_BUILD_STATIC=ON"
        "-DZLIB_INSTALL=ON"
    SENTINELS
        "lib/cmake/zlib/ZLIBConfig.cmake"
        "include/zlib.h"
        "lib/${_pans_zlib_library}"
)

# -------- yaml-cpp --------
set(_pans_yaml_source "${PANS_THIRD_PARTY_DIR}/yaml-cpp")
pans_get_source_revision("${_pans_yaml_source}" _pans_yaml_revision)
string(REPLACE "@NAME@" "yaml-cpp" _pans_yaml_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME yaml-cpp
    SOURCE_DIR "${_pans_yaml_source}"
    SOURCE_REVISION "${_pans_yaml_revision}"
    CMAKE_ARGS
        "-DBUILD_SHARED_LIBS=OFF"
        "-DYAML_BUILD_SHARED_LIBS=OFF"
        "-DYAML_CPP_BUILD_TESTS=OFF"
        "-DYAML_CPP_BUILD_TOOLS=OFF"
        "-DYAML_CPP_FORMAT_SOURCE=OFF"
        "-DYAML_CPP_INSTALL=ON"
        "-DYAML_CPP_DISABLE_UNINSTALL=ON"
    SENTINELS
        "lib/cmake/yaml-cpp/yaml-cpp-config.cmake"
        "include/yaml-cpp/yaml.h"
        "lib/${_pans_yaml_library}"
)

# -------- tinyxml2 --------
set(_pans_tinyxml2_source "${PANS_THIRD_PARTY_DIR}/tinyxml2")
pans_get_source_revision("${_pans_tinyxml2_source}" _pans_tinyxml2_revision)
string(REPLACE "@NAME@" "tinyxml2" _pans_tinyxml2_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME tinyxml2
    SOURCE_DIR "${_pans_tinyxml2_source}"
    SOURCE_REVISION "${_pans_tinyxml2_revision}"
    CMAKE_ARGS
        "-DBUILD_SHARED_LIBS=OFF"
        "-Dtinyxml2_SHARED_LIBS=OFF"
        "-Dtinyxml2_BUILD_TESTING=OFF"
    SENTINELS
        "lib/cmake/tinyxml2/tinyxml2-config.cmake"
        "include/tinyxml2.h"
        "lib/${_pans_tinyxml2_library}"
)

# -------- kcp --------
set(_pans_kcp_source "${PANS_THIRD_PARTY_DIR}/kcp")
pans_get_source_revision("${_pans_kcp_source}" _pans_kcp_revision)
file(SHA256 "${PROJECT_SOURCE_DIR}/cmake/kcp/CMakeLists.txt" _pans_kcp_wrapper_hash)
string(REPLACE "@NAME@" "kcp" _pans_kcp_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME kcp
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/cmake/kcp"
    SOURCE_REVISION "${_pans_kcp_revision}"
    KEY_MATERIAL "wrapper=${_pans_kcp_wrapper_hash}"
    CMAKE_ARGS
        "-DPANS_KCP_SOURCE_DIR=${_pans_kcp_source}"
        "-DBUILD_SHARED_LIBS=OFF"
    SENTINELS
        "lib/cmake/kcp/kcp-config.cmake"
        "include/ikcp.h"
        "lib/${_pans_kcp_library}"
)

# -------- protobuf, 依赖预编译的 Abseil 与 zlib--------
set(_pans_protobuf_source "${PANS_THIRD_PARTY_DIR}/protobuf")
pans_get_source_revision("${_pans_protobuf_source}" _pans_protobuf_revision)
string(REPLACE "@NAME@" "protobuf" _pans_protobuf_library "${_pans_static_library}")
if(MSVC)
    string(REPLACE "@NAME@" "libprotobuf" _pans_protobuf_library "${_pans_static_library}")
endif()
set(_pans_protoc "bin/protoc${CMAKE_EXECUTABLE_SUFFIX}")
pans_add_cmake_dependency(
    NAME protobuf
    SOURCE_DIR "${_pans_protobuf_source}"
    SOURCE_REVISION "${_pans_protobuf_revision}"
    KEY_MATERIAL
        "abseil=${PANS_ABSEIL_CPP_CACHE_KEY}"
        "zlib=${PANS_ZLIB_CACHE_KEY}"
    DEPENDS
        "${PANS_ABSEIL_CPP_DEPENDENCY_TARGET}"
        "${PANS_ZLIB_DEPENDENCY_TARGET}"
    CMAKE_ARGS
        "-DCMAKE_CXX_STANDARD=20"
        "-DBUILD_SHARED_LIBS=OFF"
        "-Dprotobuf_BUILD_SHARED_LIBS=OFF"
        "-Dprotobuf_BUILD_TESTS=OFF"
        "-Dprotobuf_BUILD_CONFORMANCE=OFF"
        "-Dprotobuf_BUILD_EXAMPLES=OFF"
        "-Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON"
        "-Dprotobuf_WITH_ZLIB=ON"
        "-Dabsl_DIR=${PANS_ABSEIL_CPP_INSTALL_DIR}/lib/cmake/absl"
        "-DZLIB_ROOT=${PANS_ZLIB_INSTALL_DIR}"
        "-DZLIB_INCLUDE_DIR=${PANS_ZLIB_INSTALL_DIR}/include"
        "-DZLIB_LIBRARY=${PANS_ZLIB_INSTALL_DIR}/lib/${_pans_zlib_library}"
    SENTINELS
        "lib/cmake/protobuf/protobuf-config.cmake"
        "include/google/protobuf/message.h"
        "lib/${_pans_protobuf_library}"
        "${_pans_protoc}"
)

# RapidJSON 是纯头文件库，无需生成 output 缓存。
if(NOT TARGET rapidjson::rapidjson)
    add_library(rapidjson::rapidjson INTERFACE IMPORTED GLOBAL)
    set_target_properties(rapidjson::rapidjson PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PANS_THIRD_PARTY_DIR}/rapidjson/include"
    )
endif()
message(STATUS "rapidjson: header-only; using ${PANS_THIRD_PARTY_DIR}/rapidjson/include")

add_custom_target(pans_third_party_dependencies ALL)
add_dependencies(pans_third_party_dependencies
    "${PANS_ABSEIL_CPP_DEPENDENCY_TARGET}"
    "${PANS_ZLIB_DEPENDENCY_TARGET}"
    "${PANS_YAML_CPP_DEPENDENCY_TARGET}"
    "${PANS_TINYXML2_DEPENDENCY_TARGET}"
    "${PANS_KCP_DEPENDENCY_TARGET}"
    "${PANS_PROTOBUF_DEPENDENCY_TARGET}"
)

# -------- pans 当前直接依赖 --------
find_package(Threads REQUIRED)

# sudo apt update
# sudo apt install libdw-dev
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PANS_LIBDW REQUIRED IMPORTED_TARGET libdw)
endif()

if(PANS_YAML_CPP_CACHE_COMPLETE)
    find_package(yaml-cpp CONFIG REQUIRED PATHS "${PANS_YAML_CPP_INSTALL_DIR}" NO_DEFAULT_PATH)
else()
    file(MAKE_DIRECTORY "${PANS_YAML_CPP_INSTALL_DIR}/include")
    add_library(yaml-cpp::yaml-cpp STATIC IMPORTED GLOBAL)
    set_target_properties(yaml-cpp::yaml-cpp PROPERTIES
        IMPORTED_LOCATION "${PANS_YAML_CPP_INSTALL_DIR}/lib/${_pans_yaml_library}"
        INTERFACE_COMPILE_DEFINITIONS YAML_CPP_STATIC_DEFINE
        INTERFACE_INCLUDE_DIRECTORIES "${PANS_YAML_CPP_INSTALL_DIR}/include"
    )
endif()

foreach(_pans_dependency_install_dir IN ITEMS
    "${PANS_YAML_CPP_INSTALL_DIR}"
    "${PANS_ABSEIL_CPP_INSTALL_DIR}"
    "${PANS_ZLIB_INSTALL_DIR}"
    "${PANS_PROTOBUF_INSTALL_DIR}"
    "${PANS_TINYXML2_INSTALL_DIR}"
    "${PANS_KCP_INSTALL_DIR}"
)
    install(
        DIRECTORY "${_pans_dependency_install_dir}/"
        DESTINATION "."
        PATTERN ".pans-cache-*" EXCLUDE
    )
endforeach()

install(
    DIRECTORY "${PANS_THIRD_PARTY_DIR}/rapidjson/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

unset(_pans_required_submodules)
unset(_pans_static_library)
