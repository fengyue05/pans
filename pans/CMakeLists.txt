file(GLOB_RECURSE PANS_SOURCES CONFIGURE_DEPENDS *.cpp *.cc *.c)
file(GLOB_RECURSE PANS_HEADERS CONFIGURE_DEPENDS *.h *.hpp)

if(PANS_SOURCES)
    add_library(pans STATIC ${PANS_SOURCES} ${PANS_HEADERS})
    set(PANS_USAGE_SCOPE PUBLIC)

    target_include_directories(pans PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(pans PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Werror>
    )
else()
    # No translation units remain: expose headers and usage requirements only.
    add_library(pans INTERFACE ${PANS_HEADERS})
    set(PANS_USAGE_SCOPE INTERFACE)
endif()

target_include_directories(pans
    ${PANS_USAGE_SCOPE}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(pans ${PANS_USAGE_SCOPE} pans_options) # 继承所有编译/链接选项

install(TARGETS pans
    EXPORT pansTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${PANS_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pans
)
