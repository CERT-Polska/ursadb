add_executable(ursadb Daemon.cpp
                      Daemon.h
                      NetworkService.cpp
                      NetworkService.h)
target_link_libraries(ursadb ursa)
target_clangformat_setup(ursadb)

add_executable(ursadb_new NewDatabase.cpp)
target_link_libraries(ursadb_new ursa)
target_clangformat_setup(ursadb_new)

add_executable(ursadb_trim Trim.cpp)
target_link_libraries(ursadb_trim ursa)
target_clangformat_setup(ursadb_trim)

add_executable(ursadb_test Tests.cpp)
target_link_libraries(ursadb_test ursa)
target_clangformat_setup(ursadb_test)

add_executable(ursadb_bench Benchmark.cpp)
target_link_libraries(ursadb_bench ursa)
target_clangformat_setup(ursadb_bench)
