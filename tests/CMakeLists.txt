# 두 가지 호출 모드를 모두 지원:
#  1) 루트에서 add_subdirectory(tests) — 메인 빌드와 함께
#  2) cmake -S tests -B build-tests — 가벼운 단위 테스트만 (CI용)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.16)
    project(mongglemonggle_tests CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
endif()

get_filename_component(_MONGGLE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    find_package(GTest REQUIRED)
endif()

add_executable(mongglemonggle_tests
    test_main.cpp
    test_permissions.cpp
    test_ttl_cache.cpp
    test_metrics.cpp

    # under-test sources (DB/네트워크 의존성 없는 순수 함수만)
    ${_MONGGLE_ROOT}/server/src/media/permissions.cpp
    ${_MONGGLE_ROOT}/server/src/cache/ttl_cache.cpp
    ${_MONGGLE_ROOT}/server/src/metrics/metrics.cpp
)

target_include_directories(mongglemonggle_tests PRIVATE
    ${_MONGGLE_ROOT}/server/include
)

target_link_libraries(mongglemonggle_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(mongglemonggle_tests)
