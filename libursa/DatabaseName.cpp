#include "DatabaseName.h"

#include <utility>

#include "Utils.h"
#include "spdlog/spdlog.h"

DatabaseName DatabaseName::derive(const std::string &new_type,
                                  const std::string &new_filename) const {
    return DatabaseName(db_base, new_type, id, new_filename);
}

DatabaseName DatabaseName::derive_temporary() const {
    std::string id = random_hex_string(8);
    std::string fname = "temp." + id + ".ursa";
    return DatabaseName(db_base, "temp", id, fname);
}

// TODO(unknown): convert all legacy names to DatbaseName.
DatabaseName DatabaseName::parse(fs::path db_base, const std::string &name) {
    auto first_dot = name.find('.');
    auto second_dot = name.find('.', first_dot + 1);
    if (second_dot == std::string::npos) {
        throw std::runtime_error("Invalid dataset ID found");
    }
    std::string type = name.substr(0, first_dot);
    std::string id = name.substr(first_dot + 1, second_dot - first_dot - 1);
    return DatabaseName(std::move(db_base), type, id, name);
}
