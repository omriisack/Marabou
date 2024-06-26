add_custom_target(build-unit-tests ALL)
add_dependencies(build-tests build-unit-tests)

macro(marabou_add_test test_name test_dir add_common_real add_engine_real labels)
    get_filename_component(name ${test_name} NAME_WE)
    set(test_src ${name}.cc)
    set(name "${name}${CMAKE_EXECUTABLE_SUFFIX}")

    set(test_header "${test_name}.h")
    set(test_bin_dir "${TEST_DIR}/${test_dir}")
    if(NOT EXISTS "${test_bin_dir}")
        file(MAKE_DIRECTORY "${test_bin_dir}")
    endif()
    find_package(CxxTest)

    add_custom_command(
        OUTPUT  ${test_src}
        DEPENDS ${test_header}
        COMMAND
        ${CXXTEST_TESTGEN_INTERPRETER}
        ${CXXTEST_TESTGEN_EXECUTABLE}
        ${CXXTEST_TESTGEN_ARGS} -o ${test_src} ${test_header}
    )
    set_source_files_properties(${test_src} PROPERTIES GENERATED true)

    if (${add_common_real})
        set(test_src ${test_src} ${SRCS_COMMON_REAL})
    else()
        set(test_src ${test_src} ${SRCS_COMMON_MOCK})
    endif()
    if (${add_engine_real})
        set(test_src ${test_src} ${SRCS_ENGINE_REAL})
    else()
        set(test_src ${test_src} ${SRCS_ENGINE_MOCK})
    endif()
    add_executable(${name} ${test_src} ${test_header})
    target_compile_options(${name} PRIVATE ${CXXTEST_FLAGS})
    target_include_directories(${name}  PRIVATE ${LIBS_INCLUDES}
        )
    target_link_libraries(${name}  ${MARABOU_TEST_LIB} ${CXXTEST_FLAGS})
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}> "${test_bin_dir}/${name}")
    add_test(${name} "${test_bin_dir}/${name}")
    set_tests_properties(${name} PROPERTIES LABELS "${test_dir} ${labels}")
    add_dependencies(build-tests ${name})
endmacro()

add_subdirectory(configuration)
add_subdirectory(engine)
add_subdirectory(basis_factorization)
add_subdirectory(common)
add_subdirectory(system_tests)
add_subdirectory(${INPUT_PARSERS_DIR})
add_subdirectory(query_loader)
add_subdirectory(nlr)
add_subdirectory(proofs)
add_subdirectory(cegar)
