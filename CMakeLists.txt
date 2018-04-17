cmake_minimum_required(VERSION 3.5)
project(ursadb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ursadb main.cpp Utils.cpp OnDiskDataset.cpp DatasetBuilder.cpp Database.cpp OnDiskIndex.cpp IndexBuilder.cpp MemMap.cpp Query.cpp QueryParser.cpp StringParser.cpp)

add_executable(ursadb_test Tests.cpp Utils.cpp OnDiskDataset.cpp DatasetBuilder.cpp Database.cpp OnDiskIndex.cpp IndexBuilder.cpp MemMap.cpp Query.cpp QueryParser.cpp StringParser.cpp)

target_link_libraries(ursadb -lstdc++fs -lzmq)

target_link_libraries(ursadb_test -lstdc++fs -lzmq)
