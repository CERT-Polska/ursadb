#include "DatabaseUpgrader.h"

#include <fstream>

#include "Json.h"
#include "Utils.h"
#include "spdlog/spdlog.h"

std::string extract_version(const json &dbjson) {
    return dbjson.value("version", "1.0.0");
}

json read_json(const fs::path &root, std::string_view path) {
    std::ifstream db_file(root / path, std::ifstream::binary);

    if (db_file.fail()) {
        throw std::runtime_error("Failed to open database file " +
                                 std::string(root) + " " + std::string(path));
    }

    json db_json;

    try {
        db_file >> db_json;
    } catch (json::parse_error &e) {
        throw std::runtime_error("Failed to parse JSON");
    }
    return db_json;
}

void save_json(const fs::path &root, std::string_view filename,
               const json &dbjson) {
    std::string tmp_db_name = root / ("upgrade." + random_hex_string(8));
    std::ofstream db_file;
    db_file.exceptions(std::ofstream::badbit);
    db_file.open(tmp_db_name, std::ofstream::binary);
    db_file << std::setw(4) << dbjson << std::endl;
    db_file.flush();
    db_file.close();
    fs::rename(tmp_db_name, root / filename);
}

// What changed: "max_mem_size" config options was removed, and
// iterators were added to the db.
void upgrade_v1_0_0(json *dbjson) {
    json &db = *dbjson;
    db["version"] = "1.3.2";

    if (db.find("config") != db.end()) {
        db["config"] = std::map<std::string, std::string>();
    }

    if (db["config"].find("max_mem_size") != db["config"].end()) {
        db["config"].erase("max_mem_size");
    }

    if (db["iterators"] == nullptr) {
        db["iterators"] = std::unordered_map<std::string, std::string>();
    }
}

// What changed: iterator GC was added, and all iterators now
// know when they were last read. To avoid deleting all old iterators,
// we set last read time to the current timestamp.
void upgrade_v1_3_2(const fs::path &root, json *dbjson) {
    json &db = *dbjson;
    db["version"] = "1.5.0";
    for (const auto iter : db["iterators"].items()) {
        std::string filename{iter.value()};
        json iterjson = read_json(root, filename);
        iterjson["last_read_timestamp"] = std::time(nullptr);
        save_json(root, filename, iterjson);
    }
}

void migrate_version(std::string_view path) {
    std::string most_recent = "1.5.0";
    std::string prev_version;
    auto db_root = fs::path(path).parent_path();
    auto db_name = std::string(fs::path(path).filename());
    while (true) {
        json db_json = std::move(read_json(db_root, db_name));
        std::string version = extract_version(db_json);
        if (version == prev_version) {
            spdlog::error("Upgrade procedure failed. Trying to proceed...");
            break;
        }
        if (version != prev_version && !prev_version.empty()) {
            spdlog::info("UPGRADE: {} -> {}.", prev_version, version);
        }
        if (version == most_recent) {
            break;
        }
        if (version == "1.0.0") {
            upgrade_v1_0_0(&db_json);
        }
        if (version == "1.3.2") {
            upgrade_v1_3_2(db_root, &db_json);
        }
        prev_version = version;
        save_json(db_root, db_name, db_json);
    }
}
