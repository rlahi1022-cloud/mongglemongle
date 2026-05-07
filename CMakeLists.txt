cmake_minimum_required(VERSION 3.16)
project(mongglemonggle VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/server/include)

set(SOURCES
    server/src/main.cpp
    server/src/app_config.cpp
    server/src/auth/auth_service.cpp
    server/src/entry/entry_service.cpp
    server/src/event/event_bus.cpp
    server/src/router/router.cpp
    server/src/router/routes.cpp
)

add_executable(mongglemonggle ${SOURCES})
