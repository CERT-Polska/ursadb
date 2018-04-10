cmake_minimum_required(VERSION 3.5)
project(ursadb)

set(CMAKE_CXX_STANDARD 14)

add_executable(ursadb main.cpp utils.cpp dataset.cpp dataset_builder.cpp database.cpp database.h query.cpp query.h)
