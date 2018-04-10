cmake_minimum_required(VERSION 3.5)
project(ursadb)

set(CMAKE_CXX_STANDARD 14)

add_executable(ursadb main.cpp Utils.cpp OnDiskDataset.cpp DatasetBuilder.cpp Database.cpp Database.h OnDiskIndex.cpp OnDiskIndex.h IndexBuilder.cpp IndexBuilder.h MemMap.cpp MemMap.h)
