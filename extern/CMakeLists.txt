add_library(catch INTERFACE)
target_include_directories(catch INTERFACE .)

add_library(json INTERFACE)
target_include_directories(json INTERFACE .)

add_library(pegtl INTERFACE)
target_include_directories(pegtl INTERFACE .)

# target spdlog::spdlog
add_subdirectory(spdlog)
