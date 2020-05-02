#include "DatabaseUpgrader.h"

#include <fstream>

#include "Json.h"
#include "Utils.h"
#include "spdlog/spdlog.h"

std::string extract_version(const json &dbjson) {
    return dbjson.value("version", "1.0.0");
}

json read_json(std::string_view path) {
    std::ifstream db_file(std::string(path), std::ifstream::binary);

    if (db_file.fail()) {
        throw std::runtime_error("Failed to open database file");
    }

    json db_json;

    try {
        db_file >> db_json;
    } catch (json::parse_error &e) {
        throw std::runtime_error("Failed to parse JSON");
    }
    return db_json;
}

void save_json(fs::path root, std::string_view path, const json &dbjson) {
    std::string tmp_db_name = root / ("upgrade." + random_hex_string(8));
    std::ofstream db_file;
    db_file.exceptions(std::ofstream::badbit);
    db_file.open(tmp_db_name, std::ofstream::binary);
    db_file << std::setw(4) << dbjson << std::endl;
    db_file.flush();
    db_file.close();
    fs::rename(tmp_db_name, path);
}

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

void migrate_version(std::string_view path) {
    std::string most_recent = "1.3.2";
    std::string prev_version = "";
    auto db_root = fs::path(path).parent_path();
    while (true) {
        json db_json = std::move(read_json(path));
        std::string version = extract_version(db_json);
        if (version == prev_version) {
            spdlog::error("Upgrade procedure failed. Trying to proceed...");
            break;
        } else if (version != prev_version && !prev_version.empty()) {
            spdlog::info("UPGRADE: {} -> {}.", prev_version, version);
        }
        if (version == most_recent) {
            break;
        }
        if (version == "1.0.0") {
            upgrade_v1_0_0(&db_json);
        }
        prev_version = version;
        save_json(db_root, path, db_json);
    }
}
