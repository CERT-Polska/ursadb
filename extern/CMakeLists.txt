add_library(catch INTERFACE)
target_include_directories(catch INTERFACE .)

add_library(json INTERFACE)
target_include_directories(json INTERFACE .)

add_library(pegtl INTERFACE)
target_include_directories(pegtl INTERFACE .)

# target spdlog::spdlog
add_subdirectory(spdlog EXCLUDE_FROM_ALL)

# need libsodium for curve, but curve not used in ursa.
# so, then we dont need to compile libsodium.
option(WITH_LIBSODIUM "" OFF)
option(ENABLE_CURVE "" OFF)
# websockets requires a TLS library, which we don't have handy.
# also, ursa doesn't use websockets.
# so, disable feature to hide warnings during configure.
option(ENABLE_WS "" OFF)
# remove extra zmq targets
option(BUILD_TESTS "" OFF)
add_subdirectory(libzmq EXCLUDE_FROM_ALL)

# remove extra cppzmq targets
option(CPPZMQ_BUILD_TESTS "" OFF)
add_subdirectory(cppzmq EXCLUDE_FROM_ALL)
