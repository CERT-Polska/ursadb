#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <stack>
#include <utility>
#include <variant>
#include <vector>
#include <zmq.hpp>

#include "Command.h"
#include "Database.h"
#include "DatasetBuilder.h"
#include "OnDiskDataset.h"
#include "QueryParser.h"

static bool endsWith(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

int main(int argc, char *argv[]) {
    bool dry_run = true;

    if (argc != 2 && argc != 3) {
        std::cout << "Usage: " << argv[0] << " [db_file]\n" << std::endl;
        return 1;
    }

    if (argc == 3) {
        if (strcmp(argv[1], "--confirm") == 0) {
            dry_run = false;
        } else {
            std::cout << "Invalid parameters." << std::endl;
            return 1;
        }
    }

    std::string db_name(argv[argc - 1]);
    Database db(db_name);

    std::set<std::string> db_files;
    std::vector<fs::path> remove_list;
    db_files.insert(db.get_name());

    for (OnDiskDataset *workingSet : db.working_sets()) {
        db_files.insert(workingSet->get_name());
        db_files.insert(workingSet->get_files_fname());

        for (const OnDiskIndex &index : workingSet->get_indexes()) {
            db_files.insert(index.get_fname());
        }
    }

    fs::directory_iterator end;
    unsigned int legit_files = 0;

    for (fs::directory_iterator dir(db.get_base()); dir != end; ++dir) {
        if (fs::is_regular_file(dir->path())) {
            fs::path fn = dir->path().filename();

            if (fn.string() == db.get_name()) {
                std::cout << "required file " << fn.string() << std::endl;
                legit_files++;
            } else if (endsWith(fn.string(), "." + db.get_name().string())) {
                if (db_files.find(fn.string()) == db_files.end()) {
                    std::cout << "unlinked file " << fn.string() << std::endl;
                    remove_list.push_back(dir->path());
                } else {
                    std::cout << "required file " << fn.string() << std::endl;
                    legit_files++;
                }
            }
        }
    }

    if (legit_files != db_files.size()) {
        std::cout << "something went wrong, found only " << legit_files
                  << " out of " << db_files.size() << " declared database files"
                  << std::endl;
        return 1;
    }

    if (dry_run) {
        std::cout << "in order to really remove unlinked files, execute:"
                  << std::endl;
        std::cout << "    " << argv[0] << " --confirm '" + db_name << "'"
                  << std::endl;
        std::cout << "WARNING: doing this while UrsaDB is running is strongly "
                     "discouraged and may lead to data loss"
                  << std::endl;
    }

    if (!remove_list.size()) {
        std::cout << "everything OK, database seems to be in consistent state"
                  << std::endl;
    } else {
        std::cout << "found " << remove_list.size() << " dangling files"
                  << std::endl;
    }

    if (!dry_run) {
        for (fs::path &fn : remove_list) {
            std::cout << "delete " << fs::absolute(fn).string();

            if (fs::remove(fn)) {
                std::cout << " REMOVED" << std::endl;
            } else {
                std::cout << " FAILED" << std::endl;
            }
        }
    }

    return 0;
}
