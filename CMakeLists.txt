cmake_minimum_required(VERSION 3.16)

# vcpkg manifest mode toolchain auto-detect.
# Honors -DCMAKE_TOOLCHAIN_FILE=... if the user passes one explicitly.
# Otherwise, falls back to $VCPKG_ROOT or ~/vcpkg. Only redis-plus-plus is
# pulled via vcpkg (see vcpkg.json) — all other deps come from apt.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "vcpkg toolchain")
    elseif(EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "vcpkg toolchain")
    endif()
endif()

project(mongglemonggle VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Drogon  CONFIG REQUIRED)
find_package(cpp-jwt CONFIG REQUIRED)
find_package(OpenSSL        REQUIRED)
find_package(OpenCV         REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(miniocpp CONFIG REQUIRED)

# libcrypt (libxcrypt) for bcrypt via crypt_r()
find_library(CRYPT_LIBRARY NAMES crypt REQUIRED)

add_executable(mongglemonggle
    server/src/main.cpp
    server/src/app_config.cpp
    server/src/auth/auth_service.cpp
    server/src/auth/jwt_service.cpp
    server/src/auth/password_service.cpp
    server/src/ai/ai_hub_client.cpp
    server/src/event/event_bus.cpp
    server/src/blocks/blocks_service.cpp
    server/src/cache/ttl_cache.cpp
    server/src/cache/redis_cache.cpp
    server/src/cache/layered_cache.cpp
    server/src/metrics/metrics.cpp
    server/src/comments/comments_service.cpp
    server/src/follows/follows_service.cpp
    server/src/media/media_service.cpp
    server/src/media/local_fs_storage.cpp
    server/src/media/minio_storage.cpp
    server/src/media/permissions.cpp
    server/src/media/media_queue.cpp
    server/src/middleware/cors.cpp
    server/src/middleware/rate_limiter.cpp
    server/src/middleware/request_log.cpp
    server/src/notifications/notifications_service.cpp
    server/src/notifications/stream_hub.cpp
    server/src/posts/posts_service.cpp
    server/src/posts/snapshot_service.cpp
    server/src/posts/snapshot_worker.cpp
    server/src/profile/profile_service.cpp
    server/src/router/routes.cpp
    server/src/router/auth_routes.cpp
    server/src/router/posts_routes.cpp
    server/src/router/snapshot_routes.cpp
    server/src/router/follows_routes.cpp
    server/src/router/media_routes.cpp
    server/src/router/profile_routes.cpp
    server/src/router/comments_routes.cpp
    server/src/router/notifications_routes.cpp
    server/src/router/blocks_routes.cpp
)

target_include_directories(mongglemonggle PRIVATE
    ${CMAKE_SOURCE_DIR}/server/include
)

target_link_libraries(mongglemonggle PRIVATE
    Drogon::Drogon
    cpp-jwt::cpp-jwt
    OpenSSL::Crypto
    OpenSSL::SSL
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    redis++::redis++_static
    hiredis::hiredis
    miniocpp::miniocpp
    ${CRYPT_LIBRARY}
)

# Unit tests (gated by BUILD_TESTING, default ON via CTest enable_testing)
enable_testing()
add_subdirectory(tests)
