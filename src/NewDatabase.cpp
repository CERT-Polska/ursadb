#include <array>
#include <fstream>
#include <list>
#include <stack>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "libursa/Command.h"
#include "libursa/Database.h"
#include "libursa/DatasetBuilder.h"
#include "libursa/Json.h"
#include "libursa/OnDiskDataset.h"
#include "libursa/QueryParser.h"
#include "spdlog/spdlog.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        spdlog::info("Usage:\n");
        spdlog::info("    %s [database]\n", argv[0]);
        return 1;
    }

    try {
        Database::create(argv[1]);
    } catch (const std::runtime_error &ex) {
        spdlog::error("Failed to create database: {}", ex.what());
        return 1;
    } catch (const json::exception &ex) {
        spdlog::error("Failed to create database: {}", ex.what());
        return 1;
    }

    return 0;
}
