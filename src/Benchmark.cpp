#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <chrono>
#include <ctime>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#include "libursa/Core.h"
#include "libursa/Database.h"
#include "libursa/Utils.h"

template <typename F, typename... Args>
static uint64_t benchmark_ms(F &&func, Args &&... args) {
    auto start = std::chrono::steady_clock::now();
    std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    return duration.count();
}

fs::path produce_test_files(int file_count, int file_size) {
    fs::path directory = fs::temp_directory_path();
    directory /= "ursabench_" + random_hex_string(8);
    fs::create_directory(directory);
    std::mt19937_64 random(0);
    std::uniform_int_distribution<int> pick(0, 255);
    for (int i = 0; i < file_count; i++) {
        std::string filename = std::to_string(i);
        fs::path out_path = directory / filename;
        std::ofstream out(out_path);
        for (int j = 0; j < file_size; j++) {
            out << (char)pick(random);
        }
    }
    return directory;
}

uint64_t benchmark_index(int files, int file_size) {
    fs::path test_path = produce_test_files(files, file_size);

    fs::path ursa_file = fs::temp_directory_path();
    ursa_file /= "ursabench_" + random_hex_string(8);

    Database::create(ursa_file);
    Database db(ursa_file);
    uint64_t result = benchmark_ms([&db, &test_path]() {
        auto snap = db.snapshot();
        std::vector<IndexType> index_types = {IndexType::GRAM3};
        Task *task = db.allocate_task();
        snap.recursive_index_paths(task, index_types, {test_path});
        db.commit_task(task->id);
    });
    for (auto ds : db.working_sets()) {
        db.drop_dataset(ds->get_name());
    }
    std::set<DatabaseSnapshot *> working_snapshots;
    db.collect_garbage(working_snapshots);
    fs::remove_all(test_path);
    return result;
}

void do_bench(int files, int file_size) {
    fmt::print("index({}, {}): {}\n", files, file_size,
               benchmark_index(files, file_size));
}

int main() {
    spdlog::set_level(spdlog::level::warn);
    do_bench(100, 1000000);
    do_bench(100, 1000000);
    do_bench(100, 1000000);
    return 0;
}
