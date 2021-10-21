#include <unistd.h>

#include <set>
#include <vector>

#include <zmq.hpp>
#include "spdlog/spdlog.h"

#include "Environment.h"
#include "libursa/Utils.h"
#include "libursa/Daemon.h"
#include "libursa/Command.h"
#include "libursa/Database.h"
#include "libursa/DatabaseUpgrader.h"

using namespace fmt;
using namespace std;
namespace fs = std::experimental::filesystem;

void print_usage(std::string exec_name) {
    // clang-format off
    fmt::print(stderr, "Usage: {} [option] /path/to/database\n", exec_name);
    fmt::print(stderr, "    [-1]     compact a single round, default: false\n");
    // clang-format on
}

int main(int argc, char *argv[]) {
    // path to index
    std::string arg_db_path;

    bool arg_single_compact = false;

    int c;
    while ((c = getopt(argc, argv, "1")) != -1) {
        switch (c) {
            case '1':
                arg_single_compact = true;
                break;
                print_usage(argc >= 1 ? argv[0] : "ursadb_compact");
                return 0;
            default:
                print_usage(argc >= 1 ? argv[0] : "ursadb_compact");
                spdlog::error("Failed to parse command line.");
                return 1;
        }
    }

    if (argc - optind != 1) {
        spdlog::error("Incorrect positional arguments provided.");
        print_usage(argc >= 1 ? argv[0] : "ursadb_compact");
        return 1;
    } else {
        arg_db_path = std::string(argv[optind]);
    }

    spdlog::info("UrsaDB v{}: {}", get_version_string(), arg_db_path);
    fix_rlimit();
    migrate_version(arg_db_path);

    auto round = 0;
    try {
        Database db(arg_db_path);

        while (true)  {
            auto pre_dataset_count = db.working_sets().size();

            CompactCommand cmd = CompactCommand(CompactType::Smart);
            DatabaseSnapshot snap = db.snapshot();
            std::vector<DatabaseLock> locks = dispatch_locks(cmd, &snap);

            TaskSpec* spec = db.allocate_task("compact: smart", "n/a", locks);
            Task task(spec);

            spdlog::info("JOB: {}: start: compact: smart", spec->id());
            Response resp = dispatch_command(cmd, &task, &snap);
            spdlog::info("RESP: {}", resp.to_string());
            db.commit_task(task.spec(), task.changes());
            uint64_t task_ms = get_milli_timestamp() - task.spec().epoch_ms();
            spdlog::info("JOB: {}: done ({}ms): compact: smart", task.spec().id(), task_ms);

            std::set<DatabaseSnapshot *> empty;
            db.collect_garbage(empty);

            if (arg_single_compact) {
                spdlog::info("DONE: single compaction");
                break;
            }

            auto post_dataset_count = db.working_sets().size();
            if (post_dataset_count == pre_dataset_count) {
                spdlog::info("DONE: fixed point: {} datasets", post_dataset_count);
                break;
            } else {
                spdlog::info("ROUND: {}: {} -> {} datasets", ++round, pre_dataset_count, post_dataset_count);
                continue;
            }
        }
    } catch (const std::runtime_error &ex) {
        spdlog::error("Runtime error: {}", ex.what());
        return 1;
    } catch (const json::exception &ex) {
        spdlog::error("JSON error: {}", ex.what());
        return 1;
    } catch (const zmq::error_t &ex) {
        spdlog::error("ZeroMQ error: {}", ex.what());
        return 1;
    }

    return 0;
}
