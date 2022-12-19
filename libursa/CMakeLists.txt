add_library(
    ursa
    STATIC
    BitmapIndexBuilder.cpp 
    BitmapIndexBuilder.h
    Command.h
    Core.cpp
    Core.h
    Daemon.cpp
    Daemon.h
    Database.cpp
    Database.h
    DatabaseConfig.cpp
    DatabaseConfig.h
    DatabaseLock.cpp
    DatabaseLock.h
    DatabaseName.cpp
    DatabaseName.h
    DatabaseSnapshot.cpp
    DatabaseSnapshot.h
    DatabaseUpgrader.cpp
    DatabaseUpgrader.h
    DatasetBuilder.cpp
    DatasetBuilder.h
    ExclusiveFile.cpp
    ExclusiveFile.h
    FlatIndexBuilder.cpp
    FlatIndexBuilder.h
    IndexBuilder.h
    Indexer.cpp
    Indexer.h
    Json.h
    MemMap.cpp
    MemMap.h
    OnDiskDataset.cpp
    OnDiskDataset.h
    OnDiskFileIndex.cpp
    OnDiskFileIndex.h
    OnDiskIndex.cpp
    OnDiskIndex.h
    OnDiskIterator.cpp
    OnDiskIterator.h
    QString.cpp
    QString.h
    Query.cpp
    Query.h
    QueryCounters.cpp
    QueryCounters.h
    QueryParser.cpp
    QueryParser.h
    QueryResult.cpp
    QueryResult.h
    RawFile.cpp
    RawFile.h
    Responses.cpp
    Responses.h
    ResultWriter.h
    SortedRun.cpp
    SortedRun.h
    Task.cpp
    Task.h
    Utils.cpp
    Utils.h
    Version.h.in
    ZHelpers.cpp
    ZHelpers.h
)
target_include_directories(
    ursa
    PRIVATE .
    PRIVATE ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(ursa -lstdc++fs -pthread pegtl json spdlog::spdlog)
target_link_libraries(ursa libzmq-static)
target_link_libraries(ursa cppzmq-static)
target_enable_ipo(ursa)
target_clangformat_setup(ursa)

# Suppress cppzmq deprecation warning. Remove when it's fixed
# See: https://github.com/zeromq/cppzmq/issues/330
target_compile_options(ursa PUBLIC -Wno-deprecated-declarations)

find_package(Git)
if(Git_FOUND)
    execute_process(
        COMMAND
        # get commit shorthash
        ${GIT_EXECUTABLE} rev-parse --short HEAD
        OUTPUT_VARIABLE
        COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    message(WARNING "Git not found. Hash info won't be available")
    set(COMMIT_HASH "(unknown commit)")
endif()

configure_file(Version.h.in ${PROJECT_BINARY_DIR}/generated/Version.h)
