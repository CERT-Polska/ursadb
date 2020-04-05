cmake_minimum_required(VERSION 3.5)
project(ursadb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall")

message("Looking for clang-format")
find_program(CLANG_FORMAT_EXEC clang-format)
if(NOT EXISTS ${CLANG_FORMAT_EXEC})
    message(WARNING "clang-format not found! Target 'format' won't be available")
else()
    file(GLOB FORMAT_SOURCES *.cpp *.h)
    add_custom_target(format
      COMMAND
       ${CLANG_FORMAT_EXEC}
       -style=file
       -i
       ${FORMAT_SOURCES}
      WORKING_DIRECTORY
       ${CMAKE_SOURCE_DIR}
      COMMENT
      "Formatting..."
    )
endif()

add_library(
    ursa
    STATIC
    Utils.cpp
    OnDiskDataset.cpp
    DatasetBuilder.cpp
    Database.cpp
    Trim.cpp
    OnDiskIndex.cpp
    FlatIndexBuilder.cpp
    BitmapIndexBuilder.cpp
    MemMap.cpp
    Query.cpp
    QueryParser.cpp
    ExclusiveFile.cpp
    DatabaseSnapshot.cpp
    Task.cpp
    Indexer.cpp
    DatabaseHandle.cpp
    ZHelpers.cpp
    Responses.cpp
    RawFile.cpp
    OnDiskFileIndex.cpp
)
target_link_libraries(ursa -lstdc++fs -lzmq -pthread)

add_executable(ursadb Daemon.cpp NetworkService.cpp)
target_link_libraries(ursadb ursa)

add_executable(ursadb_new NewDatabase.cpp)
target_link_libraries(ursadb_new ursa)

add_executable(ursadb_trim Trim.cpp)
target_link_libraries(ursadb_trim ursa)

add_executable(ursadb_test Tests.cpp)
target_link_libraries(ursadb_test ursa)

add_executable(ursadb_bench Benchmark.cpp)
target_link_libraries(ursadb_bench ursa)
