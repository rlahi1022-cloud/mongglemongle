find_package(GTest CONFIG REQUIRED)

# 테스트 대상 소스를 직접 컴파일해서 메인 바이너리 의존성을 끊는다.
# (DB/네트워크 없는 순수 단위 테스트만 우선)
add_executable(mongglemonggle_tests
    test_main.cpp
    test_permissions.cpp
    test_ttl_cache.cpp
    test_metrics.cpp

    # under-test sources
    ${CMAKE_SOURCE_DIR}/server/src/media/permissions.cpp
    ${CMAKE_SOURCE_DIR}/server/src/cache/ttl_cache.cpp
    ${CMAKE_SOURCE_DIR}/server/src/metrics/metrics.cpp
)

target_include_directories(mongglemonggle_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/server/include
)

target_link_libraries(mongglemonggle_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)

# CTest 통합
include(GoogleTest)
gtest_discover_tests(mongglemonggle_tests)
