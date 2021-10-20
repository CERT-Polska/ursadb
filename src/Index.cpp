#include <unistd.h>

#include <set>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
// required for GCC: 
// https://intellij-support.jetbrains.com/hc/en-us/community/posts/115000792304-C-17-cannot-include-filesystem-
#include <experimental/filesystem>

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

template<typename T>
class UnlockedBox {
   private:
    std::lock_guard<std::mutex> lock;

   public:
    T* obj;

    UnlockedBox(std::mutex& mutex, T* obj) 
        : lock(mutex), obj(obj) {}

    T* operator-> ()
    {
        return this->obj;
    }
};

template<typename T>
class LockBox {
   private:
    T obj;
    std::mutex mutex;

   public:
    LockBox(T obj) : obj(std::move(obj)) {}

    UnlockedBox<T> acquire() {
        return UnlockedBox<T>(this->mutex, &this->obj);
    }
};

using Job = std::tuple<std::shared_ptr<LockBox<Database>>, IndexCommand>;

using JobResult = std::tuple<std::unique_ptr<DatabaseSnapshot>, std::unique_ptr<Task>, Response>;

JobResult process_job(Job job) {
    auto [ db_lock, cmd ] = std::move(job);

    std::unique_ptr<DatabaseSnapshot> snap;
    {
        auto db = db_lock->acquire();
        snap = std::unique_ptr<DatabaseSnapshot>(new DatabaseSnapshot(db->snapshot()));
    }

    std::vector<DatabaseLock> locks = dispatch_locks(cmd, &*snap);

    std::ostringstream task_id;
    task_id << "index " << cmd.get_paths().size() << " files, starting with " << cmd.get_paths()[0];
    TaskSpec* spec;
    {
        auto db = db_lock->acquire();
        spec = db->allocate_task(task_id.str(), "n/a", locks);
    }
    std::unique_ptr<Task> task = std::make_unique<Task>(spec);

    spdlog::info("JOB: {}: start: {}", spec->id(), task_id.str());
    Response resp = dispatch_command(cmd, &*task, &*snap);

    return std::make_tuple(std::move(snap), std::move(task), resp);
}

// collect all the absolute, canonical files in the given directory (recursive).
std::vector<std::string> collect_file_paths(std::string path) {
    std::vector<std::string> ret;

    for (auto entry : fs::recursive_directory_iterator(path)) {
        if (fs::is_regular_file(entry.path())) {
            ret.push_back(fs::canonical(entry.path()).string());
        }
    }

    return ret;
}

void print_usage(std::string exec_name) {
    // clang-format off
    fmt::print(stderr, "Usage: {} [option] /path/to/database /path/to/index\n", exec_name);
    fmt::print(stderr, "    [-w <workers>]     number of workers to use, default: 4\n");
    fmt::print(stderr, "    [-i <index_type>]  (multi) type of index, default: gram3, text4, wide8, hash4\n");
    fmt::print(stderr, "    [-t <tag>]         (multi) tag to apply to samples, default: none\n");
    // clang-format on
}

int main(int argc, char *argv[]) {
    // number of database workers
    uint64_t arg_workers = 4;

    // index types, including gram3, text4, wide8, hash4
    std::set<std::string> arg_types;

    // tags to apply to samples
    std::set<std::string> arg_tags;

    // path to index
    std::string arg_db_path;

    // path to samples
    std::string arg_samples_path;

    int c;
    while ((c = getopt(argc, argv, "w:t:i:")) != -1) {
        switch (c) {
            case 'w':
                arg_workers = atoi(optarg);
                break;
            case 't':
                arg_tags.insert(optarg);
                break;
            case 'i':
                // TODO: validate index types here
                arg_types.insert(optarg);
                break;
            case 'h':
                print_usage(argc >= 1 ? argv[0] : "ursadb_index");
                return 0;
            default:
                print_usage(argc >= 1 ? argv[0] : "ursadb_index");
                spdlog::error("Failed to parse command line.");
                return 1;
        }
    }

    if (argc - optind != 2) {
        spdlog::error("Incorrect positional arguments provided.");
        print_usage(argc >= 1 ? argv[0] : "ursadb_index");
        return 1;
    } else {
        arg_db_path = std::string(argv[optind]);
        arg_samples_path = std::string(argv[optind + 1]);
    }

    if (arg_types.size() == 0) {
        arg_types.insert(std::string("gram3"));
        arg_types.insert(std::string("text4"));
        arg_types.insert(std::string("wide8"));
        arg_types.insert(std::string("hash4"));
    }

    spdlog::info("db: {}", arg_db_path);
    spdlog::info("samples: {}", arg_samples_path);
    spdlog::info("workers: {}", arg_workers);
    std::vector<IndexType> types;
    for (auto arg_type : arg_types) {
        spdlog::info("type: {}", arg_type);
        types.push_back(*index_type_from_string(arg_type));
    }
    std::set<std::string> taints;
    for (auto arg_tag : arg_tags) {
        spdlog::info("tag: {}", arg_tag);
        taints.insert(arg_tag);
    }

    spdlog::info("UrsaDB v{}", get_version_string());
    fix_rlimit();
    migrate_version(arg_db_path);

    try {
        auto db = Database(arg_db_path);
        auto db_lock = std::shared_ptr<LockBox<Database>>(new LockBox<Database>(std::move(db)));

        std::vector<std::string> found_sample_paths = collect_file_paths(arg_samples_path);
        sort(found_sample_paths.begin(), found_sample_paths.end());

        for (auto path : found_sample_paths) {
            spdlog::info("path: {}", path);
        }

        std::vector<std::vector<std::string>> batches;

        // TODO: make this configurable
        uint64_t batch_size = 2;
        std::vector<std::string>::iterator batch_start;
        for (batch_start = found_sample_paths.begin(); (batch_start + batch_size) < found_sample_paths.end(); batch_start += batch_size) {
            auto batch_end = batch_start + batch_size;
            std::vector<std::string> batch;
            copy(batch_start, batch_end, std::back_inserter(batch));
            batches.push_back(batch);
        }
        if (batch_start != found_sample_paths.end()) {
            auto batch_end = found_sample_paths.end();
            std::vector<std::string> batch;
            copy(batch_start, batch_end, std::back_inserter(batch));
            batches.push_back(batch);
        }

        auto job_queue_lock = LockBox<std::queue<Job>>(std::queue<Job>());
        auto result_queue_lock = LockBox<std::queue<JobResult>>(std::queue<JobResult>());
        for (auto batch : batches) {

            spdlog::info("batch");
            for (auto path : batch) {
                spdlog::info("  - {}", path);
            }

            IndexCommand cmd = IndexCommand(batch, types, taints, true);
            Job job = std::make_tuple(db_lock, cmd);

            {
                auto job_queue = job_queue_lock.acquire();
                job_queue->push(job);
            }
        }

        // TODO: thread pool
        while (!job_queue.empty()) {
            auto job = [&job_queue_lock](){
                auto job_queue = job_queue_lock.acquire();
                auto job = job_queue->front();
                job_queue->pop();
                return job;
            }();

            auto [ snap, task, resp ] = process_job(job);
            spdlog::info("RESP: {}", resp.to_string());

            uint64_t task_ms = get_milli_timestamp() - task->spec().epoch_ms();
            spdlog::info("JOB: {}: done ({}ms): {}", task->spec().id(), task_ms, task->spec().request_str());

            JobResult res = std::make_tuple(std::move(snap), std::move(task), resp);

            {
                auto result_queue = result_queue_lock.acquire();
                result_queue->push(std::move(res));
            }
        }

        // TODO: while not all worker threads have joined
        //
        // this block is not thread safe (i think)
        // so should be run from the main thread.
        while (!result_queue.empty()) {
            auto [ snap, task, resp ] = [&result_queue_lock](){
                auto result_queue = result_queue_lock.acquire();
                auto res = std::move(result_queue->front());
                result_queue->pop();
                return std::move(res);
            }();

            {
                auto db = db_lock->acquire();
                db->commit_task(task->spec(), task->changes());
            }

            // TODO: need to lease the snapshots and maintain a live-list.
            // db.collect_garbage(working_snapshots);
        }

        // TODO: compact

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
