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

void save_json(std::string_view path, const json &dbjson) {
    std::string tmp_db_name = "tmp-" + random_hex_string(8);
    std::ofstream db_file;
    db_file.exceptions(std::ofstream::badbit);
    db_file.open(tmp_db_name, std::ofstream::binary);
    db_file << std::setw(4) << dbjson << std::endl;
    db_file.flush();
    db_file.close();
    fs::rename(tmp_db_name, path);
}

void upgrade_v1_0_0(json *dbjson) { (*dbjson)["version"] = "1.3.2"; }

void migrate_version(std::string_view path) {
    std::string most_recent = "1.3.2";
    std::string prev_version = "";
    while (true) {
        json db_json = std::move(read_json(path));
        std::string version = extract_version(db_json);
        if (version == prev_version) {
            spdlog::error("Upgrade procedure failed. Trying to proceed...");
            break;
        } else if (version != prev_version && !prev_version.empty()) {
            spdlog::info("Upgraded storage {} -> {}.", prev_version, version);
        }
        if (version == most_recent) {
            break;
        }
        if (version == "1.0.0") {
            upgrade_v1_0_0(&db_json);
        }
        prev_version = version;
        save_json(path, db_json);
    }
}
