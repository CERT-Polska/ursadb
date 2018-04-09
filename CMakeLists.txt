cmake_minimum_required(VERSION 3.10)
project(ursadb)

set(CMAKE_CXX_STANDARD 14)

add_executable(ursadb main.cpp dataset.cpp dataset_builder.cpp)