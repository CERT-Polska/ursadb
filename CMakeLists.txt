cmake_minimum_required(VERSION 3.5)
project(ursadb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ursalib STATIC Utils.cpp OnDiskDataset.cpp DatasetBuilder.cpp Database.cpp OnDiskIndex.cpp IndexBuilder.cpp MemMap.cpp Query.cpp QueryParser.cpp)
target_link_libraries(ursalib -lstdc++fs -lzmq) 

add_executable(ursadb main.cpp)
target_link_libraries(ursadb ursalib)

add_executable(ursadb_test Tests.cpp)
target_link_libraries(ursadb_test ursalib)
