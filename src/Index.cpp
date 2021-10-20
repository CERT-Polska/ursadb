#include <unistd.h>

#include <set>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <optional>
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

class SnapshotLease {
   private:
    uint64_t _id;

   public:
    std::shared_ptr<DatabaseSnapshot> snap;
    SnapshotLease(std::shared_ptr<DatabaseSnapshot> snap, uint64_t id)
       : snap(snap), _id(id) {}

    SnapshotLease() : snap(nullptr), _id(-1) {}

    DatabaseSnapshot& operator* () {
        return *this->snap;
    }

    uint64_t id() {
        return this->_id;
    }
};

class DatabaseContext {
   private:
    std::map<uint64_t, std::shared_ptr<DatabaseSnapshot>> snaps;
    uint64_t counter;

   public:
    Database db;
    DatabaseContext(Database db)
        : db(std::move(db)), counter(0) {}

    SnapshotLease lease_snapshot() {
        auto id = counter++;
        auto snap = std::make_shared<DatabaseSnapshot>(db.snapshot());
        this->snaps[id] = snap;
        return SnapshotLease(snap, id);
    }

    void return_snapshot(SnapshotLease lease) {
        if (lease.id() == -1) {
            spdlog::critical("invalid lease id");
        } else {
            this->snaps.erase(lease.id());
        }
    }

    std::set<DatabaseSnapshot *> leased_snapshots() {
        auto ret = std::set<DatabaseSnapshot *>();

        for (auto [ id, lease ] : this->snaps) {
            ret.insert(lease.get());
        }

        return ret;
    }

    uint64_t size() {
        return this->snaps.size();
    }
};

using Job = std::tuple<std::shared_ptr<LockBox<DatabaseContext>>, IndexCommand>;

using JobResult = std::tuple<SnapshotLease, std::unique_ptr<Task>, Response>;

JobResult process_job(Job job) {
    auto [ ctx_lock, cmd ] = std::move(job);

    SnapshotLease snap;
    {
        auto ctx = ctx_lock->acquire();
        snap = ctx->lease_snapshot();
    }

    std::vector<DatabaseLock> locks = dispatch_locks(cmd, &*snap);

    std::ostringstream task_id;
    task_id << "index " << cmd.get_paths().size() << " files, starting with " << cmd.get_paths()[0];
    TaskSpec* spec;
    {
        auto ctx = ctx_lock->acquire();
        spec = ctx->db.allocate_task(task_id.str(), "n/a", locks);
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
    fmt::print(stderr, "    [-b <batch_size>]  number of samples per batch, default: 1000\n");
    fmt::print(stderr, "    [-i <index_type>]  (multi) type of index, default: gram3, text4, wide8, and hash4\n");
    fmt::print(stderr, "    [-t <tag>]         (multi) tag to apply to samples, default: none\n");
    // clang-format on
}

int main(int argc, char *argv[]) {
    // number of samples per batch
    uint64_t arg_batch_size = 1000;

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
    while ((c = getopt(argc, argv, "w:t:i:b:")) != -1) {
        switch (c) {
            case 'b':
                arg_batch_size = atoi(optarg);
                break;
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
        auto ctx_lock = std::shared_ptr<LockBox<DatabaseContext>>(new LockBox<DatabaseContext>(DatabaseContext(std::move(db))));

        std::vector<std::string> found_sample_paths = collect_file_paths(arg_samples_path);
        sort(found_sample_paths.begin(), found_sample_paths.end());

        std::vector<std::vector<std::string>> batches;
        std::vector<std::string>::iterator batch_start;
        for (batch_start = found_sample_paths.begin(); batch_start < found_sample_paths.end(); batch_start += arg_batch_size) {
            auto batch_end = batch_start + arg_batch_size;
            if (batch_end > found_sample_paths.end()) {
                batch_end = found_sample_paths.end();
            }
            std::vector<std::string> batch;
            copy(batch_start, batch_end, std::back_inserter(batch));
            batches.push_back(batch);
        }

        auto job_count = 0;
        auto job_queue_lock = LockBox<std::queue<Job>>(std::queue<Job>());
        auto result_queue_lock = LockBox<std::queue<JobResult>>(std::queue<JobResult>());
        for (auto batch : batches) {
            IndexCommand cmd = IndexCommand(batch, types, taints, true);
            Job job = std::make_tuple(ctx_lock, cmd);

            {
                auto job_queue = job_queue_lock.acquire();
                job_queue->push(job);
            }

            job_count += 1;
        }

        // thread pool that pulls from job_queue, 
        // invokes process_job, and
        // places result in result_queue.
        vector<std::thread> threads;
        for (auto i = 0; i < arg_workers; i++) {
            threads.push_back(std::thread(
                [&job_queue_lock, &result_queue_lock](){
                    while (true) {
                        Job* job = [&job_queue_lock](){
                            auto job_queue = job_queue_lock.acquire();

                            if (job_queue->empty()) {
                                return static_cast<Job *>(nullptr);
                            }

                            auto job = job_queue->front();
                            job_queue->pop();
                            return new Job(job);
                        }();

                        if (job == nullptr) {
                            // case: thread pool is empty
                            // ASSUMPTION: the queue is pre-filled before spawning workers,
                            // so the queue size only decreases.
                            // therefore, if the queue was empty before, it will empty in the future,
                            // and the worker will never have work to do.
                            // worker is done.
                            break;
                        }

                        // case: we  have a job
                        auto [ snap, task, resp ] = process_job(*job);
                        spdlog::info("RESP: {}", resp.to_string());

                        uint64_t task_ms = get_milli_timestamp() - task->spec().epoch_ms();
                        spdlog::info("JOB: {}: done ({}ms): {}", task->spec().id(), task_ms, task->spec().request_str());

                        JobResult res = std::make_tuple(std::move(snap), std::move(task), resp);

                        {
                            auto result_queue = result_queue_lock.acquire();
                            result_queue->push(std::move(res));
                        }
                    }
                }
            ));
        }

        // pull rules from result_queue and commit changes to db.
        // stop after the number of results matches the number of jobs.
        auto result_count = 0;
        while (result_count < job_count) {
            bool is_empty = true;
            {
                auto result_queue = result_queue_lock.acquire();
                is_empty = result_queue->empty();
            }
            if (is_empty) {
                sleep(0.1);
                // poll again shortly
                continue;
            }

            // ASSUMPTION: the workers only place items into the queue
            // and this is the only consumer.
            // so if the queue was non-empty above, its still non-empty here.
            auto [ snap, task, resp ] = [&result_queue_lock](){
                auto result_queue = result_queue_lock.acquire();
                auto res = std::move(result_queue->front());
                result_queue->pop();
                return std::move(res);
            }();

            {
                auto ctx = ctx_lock->acquire();
                ctx->db.commit_task(task->spec(), task->changes());
                ctx->return_snapshot(std::move(snap));

                auto snaps = ctx->leased_snapshots();
                ctx->db.collect_garbage(snaps);
            }

            result_count += 1;
        }

        for (auto &t : threads) {
            t.join();
        }

        // compact database
        {
            CompactCommand cmd = CompactCommand(CompactType::Smart);

            SnapshotLease snap;
            {
                auto ctx = ctx_lock->acquire();
                snap = ctx->lease_snapshot();
            }

            std::vector<DatabaseLock> locks = dispatch_locks(cmd, &*snap);

            TaskSpec* spec;
            {
                auto ctx = ctx_lock->acquire();
                spec = ctx->db.allocate_task("compact: smart", "n/a", locks);
            }
            std::unique_ptr<Task> task = std::make_unique<Task>(spec);

            spdlog::info("JOB: {}: start: compact: smart", spec->id());
            Response resp = dispatch_command(cmd, &*task, &*snap);
            spdlog::info("RESP: {}", resp.to_string());

            uint64_t task_ms = get_milli_timestamp() - task->spec().epoch_ms();
            spdlog::info("JOB: {}: done ({}ms): compact: smart", task->spec().id(), task_ms);

            {
                auto ctx = ctx_lock->acquire();
                ctx->db.commit_task(task->spec(), task->changes());
            }
        }

        // collect garbage
        {
            auto ctx = ctx_lock->acquire();

            spdlog::info("JOB: -1: start: collect garbage");

            auto ts1 = get_milli_timestamp();
            auto empty = std::set<DatabaseSnapshot*>();
            ctx->db.collect_garbage(empty);
            auto ts2 = get_milli_timestamp();

            uint64_t task_ms = ts2 - ts1;
            spdlog::info("JOB: -1: done ({}ms): collect garbage", task_ms);
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
