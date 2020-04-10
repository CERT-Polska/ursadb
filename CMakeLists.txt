cmake_minimum_required(VERSION 3.5)
project(
    ursadb
    VERSION 1.3.0
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(ClangFormat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libursa)

add_executable(
    ursadb
    src/Daemon.cpp
    src/Daemon.h
    src/NetworkService.cpp
    src/NetworkService.h
)
target_include_directories(ursadb PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb ursa)

add_executable(ursadb_new src/NewDatabase.cpp)
target_include_directories(ursadb_new PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_new ursa)

add_executable(ursadb_trim src/Trim.cpp)
target_include_directories(ursadb_trim PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_trim ursa)

add_executable(ursadb_test src/Tests.cpp)
target_include_directories(ursadb_test PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_test ursa)

add_executable(ursadb_bench src/Benchmark.cpp)
target_include_directories(ursadb_bench PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_bench ursa)