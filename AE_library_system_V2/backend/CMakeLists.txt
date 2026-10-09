cmake_minimum_required(VERSION 3.16)
project(LibraryManagementSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# Collect source files
set(SOURCES
    main.cpp
    src/database.cpp
    services/LibraryService.cpp
)

add_executable(library_server ${SOURCES})

target_include_directories(library_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/models
    ${CMAKE_SOURCE_DIR}/services
    ${Boost_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(library_server
    ${Boost_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

# nlohmann/json (header-only)
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(library_server nlohmann_json::nlohmann_json)
endif()
