#pragma once

#include <experimental/filesystem>

// TODO get rid of this define
namespace fs = std::experimental::filesystem;

// Represents a name in the database. Usually a filename of the form
// "type.id.dbname", for exmaple "set.23381d1f.db.ursa".
// In case of manual rename, filename may not match this format.
class DatabaseName {
    fs::path db_base;
    std::string type;
    std::string id;
    std::string filename;

   public:
    DatabaseName(fs::path db_base, std::string type, std::string id,
                 std::string filename)
        : db_base(db_base), type(type), id(id), filename(filename) {}

    DatabaseName derive(const std::string &new_type,
                        const std::string &new_filename) const;

    DatabaseName derive_temporary() const;

    std::string get_id() const { return id; }

    fs::path get_full_path() const { return db_base / filename; }

    std::string get_filename() const { return filename; }

    // TODO: this method should not exist, convert all legacy name occurences
    // to DatbaseName.
    static DatabaseName parse(fs::path db_base, std::string name);
};
