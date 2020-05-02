cmake_minimum_required(VERSION 3.9)
project(
    ursadb
    VERSION 1.3.2
    LANGUAGES CXX C
)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(ClangFormat)
include(CheckIPOSupported)
include(IPOEnableOnRelease)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(extern)
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
target_enable_ipo(ursadb)
target_clangformat_setup(ursadb)

add_executable(ursadb_new src/NewDatabase.cpp)
target_include_directories(ursadb_new PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_new ursa)
target_enable_ipo(ursadb_new)
target_clangformat_setup(ursadb_new)

add_executable(
    ursacli
    src/Client.cpp
    src/Client.h
)
target_include_directories(ursacli PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursacli ursa)
target_enable_ipo(ursacli)
target_clangformat_setup(ursacli)

add_executable(ursadb_trim src/Trim.cpp)
target_include_directories(ursadb_trim PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_trim ursa)
target_enable_ipo(ursadb_trim)
target_clangformat_setup(ursadb_trim)

add_executable(ursadb_test src/Tests.cpp)
target_include_directories(ursadb_test PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_test ursa)
target_enable_ipo(ursadb_test)
target_clangformat_setup(ursadb_test)

add_executable(ursadb_bench src/Benchmark.cpp)
target_include_directories(ursadb_bench PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ursadb_bench ursa)
target_enable_ipo(ursadb_bench)
target_clangformat_setup(ursadb_bench)

install(TARGETS
        ursadb
        ursadb_new
        ursadb_trim
        ursadb_test
        ursadb_bench
        ursacli
        DESTINATION bin)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "info@cert.pl")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libzmq5")
include(CPack)
