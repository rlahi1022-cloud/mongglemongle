cmake_minimum_required(VERSION 3.16)
project(mongglemonggle VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/app_config.cpp
    src/auth_service.cpp
    src/event_bus.cpp
    src/entry_service.cpp
    src/router.cpp
    src/routes.cpp
)

add_executable(mongglemonggle ${SOURCES})
