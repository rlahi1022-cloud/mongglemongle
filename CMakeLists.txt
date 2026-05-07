cmake_minimum_required(VERSION 3.16)
project(mongglemonggle VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Drogon CONFIG REQUIRED)

add_executable(mongglemonggle
    server/src/main.cpp
    server/src/app_config.cpp
    server/src/auth/auth_service.cpp
    server/src/entry/entry_service.cpp
    server/src/event/event_bus.cpp
    server/src/router/routes.cpp
)

target_include_directories(mongglemonggle PRIVATE
    ${CMAKE_SOURCE_DIR}/server/include
)

target_link_libraries(mongglemonggle PRIVATE Drogon::Drogon)
