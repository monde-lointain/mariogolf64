# Tests of the port's own libraries, run on the host with CTest. They link
# the libraries alone, with test doubles for the program's parts that the
# libraries call.

add_library(mg64_test_support STATIC test_support.c)
target_include_directories(mg64_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mg64_test_support PRIVATE m)

function(mg64_add_port_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE mg64_test_support ${arg_LIBRARIES} mg64_port_warnings)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

mg64_add_port_test(os_threads_test
    SOURCES os_threads_test.c
    LIBRARIES mg64_ultra SDL3::SDL3
)

# The game's own fixed-point conversions, which the gu routines call
mg64_add_port_test(gu_test
    SOURCES gu_test.c "${PROJECT_SOURCE_DIR}/src/math/matrix_conversion.c"
    LIBRARIES mg64_gu
)

mg64_add_port_test(nusys_test
    SOURCES nusys_test.c
    LIBRARIES mg64_nusys mg64_ultra SDL3::SDL3
)
