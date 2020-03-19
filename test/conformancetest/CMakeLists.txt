set(CONFORMANCETEST_SOURCES
        conformance.cpp)

add_executable(conformancetest ${CONFORMANCETEST_SOURCES})
target_link_libraries(conformancetest ${TEST_LIBRARIES})

add_dependencies(tests conformancetest)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments -fcolor-diagnostics")
    endif()
endif(CCACHE_FOUND)

set_property(DIRECTORY PROPERTY COMPILE_OPTIONS ${EXTRA_CXX_FLAGS})

add_test(NAME conformancetest
        COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/perftest
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)