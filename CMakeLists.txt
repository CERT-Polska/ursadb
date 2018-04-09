cmake_minimum_required(VERSION 3.9)
project(ursadb)

set(CMAKE_CXX_STANDARD 14)

add_executable(ursadb main.cpp dataset.cpp dataset_builder.cpp database.cpp database.h query.cpp query.h)