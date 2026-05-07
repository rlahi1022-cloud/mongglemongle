cmake_minimum_required(VERSION 3.16)
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

# libcrypt (libxcrypt) for bcrypt via crypt_r()
find_library(CRYPT_LIBRARY NAMES crypt REQUIRED)

add_executable(mongglemonggle
    server/src/main.cpp
    server/src/app_config.cpp
    server/src/auth/auth_service.cpp
    server/src/auth/jwt_service.cpp
    server/src/auth/password_service.cpp
    server/src/event/event_bus.cpp
    server/src/follows/follows_service.cpp
    server/src/media/media_service.cpp
    server/src/middleware/cors.cpp
    server/src/middleware/rate_limiter.cpp
    server/src/posts/posts_service.cpp
    server/src/posts/snapshot_service.cpp
    server/src/router/routes.cpp
    server/src/router/auth_routes.cpp
    server/src/router/posts_routes.cpp
    server/src/router/snapshot_routes.cpp
    server/src/router/follows_routes.cpp
    server/src/router/media_routes.cpp
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
    ${CRYPT_LIBRARY}
)
