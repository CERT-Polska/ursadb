#include <experimental/filesystem>
#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <chrono>
#include <ctime>
#include <unistd.h>

#include "Database.h"
#include "Core.h"
#include "Utils.h"

template<typename F, typename ...Args>
static uint64_t benchmark_ms(F&& func, Args&&... args)
{
    auto start = std::chrono::steady_clock::now();
    std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>
                        (std::chrono::steady_clock::now() - start);
    return duration.count();
}

fs::path produce_test_files(int file_count, int file_size) {
    fs::path directory = fs::temp_directory_path();
    directory /= "ursabench_" + std::to_string(std::time(nullptr));
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

    char fn[] = "/tmp/ursa_bench_XXXXXX";
    int fd = mkstemp(fn);

    if (fd == 0 || fd == -1) {
        throw std::runtime_error("failed to create random_fname");
    }

    std::string db_path = std::string(fn);
    Database::create(db_path);
    Database db(db_path);
    uint64_t result = benchmark_ms([&db, &test_path]() {
        auto snap = db.snapshot();
        std::vector<IndexType> index_types = { IndexType::GRAM3 };
        Task *task = db.allocate_task();
        snap.index_path(task, index_types, { test_path });
        db.commit_task(task->id);
    });
    close(fd);
    // TODO(xmsm) - remove test files here
    return result;
}

int main() {
    uint64_t time_index_small = benchmark_index(1, 100);
    uint64_t time_index_mid = benchmark_index(100, 1000000);
    std::cout << "index_small " << time_index_small << std::endl;
    std::cout << "index_mid " << time_index_mid << std::endl;
}
