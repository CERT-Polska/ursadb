cmake_minimum_required(VERSION 3.5)
project(ursadb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ursa STATIC Utils.cpp OnDiskDataset.cpp DatasetBuilder.cpp Database.cpp OnDiskIndex.cpp IndexBuilder.cpp MemMap.cpp Query.cpp QueryParser.cpp ExclusiveFile.cpp)
target_link_libraries(ursa -lstdc++fs -lzmq)

add_executable(ursadb Daemon.cpp)
target_link_libraries(ursadb ursa)

add_executable(ursadb_new NewDatabase.cpp)
target_link_libraries(ursadb_new ursa)

add_executable(ursadb_test Tests.cpp)
target_link_libraries(ursadb_test ursa)
