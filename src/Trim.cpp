#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <stack>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "Environment.h"
#include "libursa/Command.h"
#include "libursa/Database.h"
#include "libursa/DatasetBuilder.h"
#include "libursa/OnDiskDataset.h"
#include "libursa/QueryParser.h"
#include "spdlog/spdlog.h"

static bool endswith(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

int main(int argc, char *argv[]) {
    bool dry_run = true;

    if (argc != 2 && argc != 3) {
        spdlog::error("Usage: {} [db_file]", argv[0]);
        return 1;
    }

    if (argc == 3) {
        if (argv[1] == std::string{"--confirm"}) {
            dry_run = false;
        } else {
            spdlog::error("Invalid parameters");
            return 1;
        }
    }

    std::string db_name(argv[argc - 1]);
    Database db(db_name);

    std::set<std::string> db_files;
    std::vector<fs::path> remove_list;
    db_files.insert(db.get_name());

    for (OnDiskDataset *working_set : db.working_sets()) {
        db_files.insert(working_set->get_name());
        db_files.insert(working_set->get_files_fname());
        db_files.insert(working_set->get_fname_cache_fname());

        for (const OnDiskIndex &index : working_set->get_indexes()) {
            db_files.insert(index.get_fname());
            db_files.insert(index.get_fname());
        }
    }

    for (const auto &[id, iter] : db.get_iterators()) {
        db_files.insert(iter.get_name().get_filename());
    }

    fs::directory_iterator end;
    uint32_t legit_files = 0;

    for (fs::directory_iterator dir(db.get_base()); dir != end; ++dir) {
        if (fs::is_regular_file(dir->path())) {
            fs::path fn = dir->path().filename();

            if (fn != db.get_name() &&
                !endswith(fn.string(), "." + db.get_name().string())) {
                spdlog::warn("Unexpected: {}", fn.string());
                continue;
            }

            if (db_files.find(fn.string()) == db_files.end()) {
                spdlog::info("Orphan: {}", fn.string());
                remove_list.push_back(dir->path());
            } else {
                legit_files++;
            }
        }
    }

    if (legit_files != db_files.size()) {
        spdlog::warn("Couldn't find all required database files ({}/{})",
                     legit_files, db_files.size());
        return 1;
    }

    if (!remove_list.size()) {
        spdlog::info("Database consistent, nothing to do");
        return 0;
    }

    spdlog::warn("Found {} dangling files", remove_list.size());

    if (dry_run) {
        spdlog::info("Dry run finished. To remove unlinked files run:");
        spdlog::info("{} --confirm {}", argv[0], db_name);
        spdlog::warn("Remember to turn off ursadb before doing this");
        return 0;
    }

    for (fs::path &fn : remove_list) {
        if (fs::remove(fn)) {
            spdlog::info("Removed {}", fn.string());
        } else {
            spdlog::error("Failed to remove {}", fn.string());
        }
    }

    return 0;
}
