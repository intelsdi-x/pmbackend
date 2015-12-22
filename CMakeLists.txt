cmake_minimum_required (VERSION 2.6)

project (pmbackend)

SET(CMAKE_C_FLAGS "-std=gnu99 -O2 -ffast-math -DSRCVERSION=\\\"1.0.0\\\" -fPIC")

add_library(libpmem STATIC IMPORTED)
set_property(TARGET libpmem PROPERTY IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/nvml/src/nondebug/libpmem.a)

include_directories(include
        src nvml/src/common
        nvml/src/include
        tests/unit_tests)

add_library(pmbackend STATIC
        nvml/src/common/util.c
        nvml/src/common/out.c
        nvml/src/common/set.c
        src/backend.c
        src/caslist.c
        src/pmbackend.c
        src/tx_log.c)

add_custom_command(TARGET pmbackend PRE_BUILD
        COMMAND make -C $(CMAKE_SOURCE_DIR)/nvml/src libpmem)

add_custom_command(TARGET pmbackend POST_BUILD
        COMMAND cp $(CMAKE_SOURCE_DIR)/nvml/src/nondebug/libpmem.a $(CMAKE_BINARY_DIR)
        COMMAND ar -x libpmem.a
        COMMAND ar -x libpmbackend.a
        COMMAND ar -rc libpmbackend.a *.o
        COMMAND rm -rf *.o libpmem.a)

enable_testing()
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
include_directories(${GTEST_INCLUDE_DIRECTORIES})

add_executable(tests_runner
        tests/runner.cc
        tests/fuzzing.cc
        tests/unit_tests/unit_test_utils.cc
        tests/unit_tests/pmb_iter_close.cc
        tests/unit_tests/pmb_iter_get.cc
        tests/unit_tests/pmb_iter_next.cc
        tests/unit_tests/pmb_iter_open.cc
        tests/unit_tests/pmb_iter_pos.cc
        tests/unit_tests/pmb_iter_valid.cc
        tests/unit_tests/pmb_open.cc
        tests/unit_tests/pmb_resolve_conflict.cc
        tests/unit_tests/pmb_tdel.cc
        tests/unit_tests/pmb_tput.cc
        tests/unit_tests/pmb_tput_meta.cc
        tests/unit_tests/pmb_tx_abort.cc
        tests/unit_tests/pmb_tx_begin.cc
        tests/unit_tests/pmb_tx_commit.cc
        tests/unit_tests/pmb_tx_execute.cc)

target_link_libraries(tests_runner ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} pmbackend -luuid)

add_test(unit_tests tests_runner)

install (TARGETS pmbackend DESTINATION lib)
install (FILES "${CMAKE_SOURCE_DIR}/include/pmbackend.h" DESTINATION include)
