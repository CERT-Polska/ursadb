#include "OnDiskIterator.h"
#include "Json.h"

void write_itermeta(
    DatabaseName target,
    uint64_t byte_offset,
    uint64_t file_offset,
    uint64_t total_files,
    std::string backing_storage
) {
    DatabaseName tmp_name = target.derive_temporary();
    std::ofstream iter_file;
    iter_file.exceptions(std::ofstream::badbit);
    iter_file.open(tmp_name.get_full_path(), std::ofstream::binary);

    json iter_json;
    iter_json["byte_offset"] = byte_offset;
    iter_json["file_offset"] = file_offset;
    iter_json["total_files"] = total_files;
    iter_json["backing_storage"] = backing_storage;

    iter_file << std::setw(4) << iter_json << std::endl;
    iter_file.close();

    fs::rename(tmp_name.get_full_path(), target.get_full_path());
}

void OnDiskIterator::save() {
    write_itermeta(name, byte_offset, file_offset, total_files, datafile_filename);
}

void OnDiskIterator::drop() {
    DatabaseName datafile_name = name.derive("iterator", datafile_filename);
    fs::remove(name.get_full_path());
    fs::remove(datafile_name.get_full_path());
}

OnDiskIterator::OnDiskIterator(const DatabaseName &name) :name(name) {
    std::ifstream in(name.get_full_path(), std::ifstream::binary);
    json j;
    in >> j;

    byte_offset = j["byte_offset"];
    file_offset = j["file_offset"];
    total_files = j["total_files"];
    datafile_filename = j["backing_storage"];
}

void OnDiskIterator::pop(int count, std::vector<std::string> *out) {
    DatabaseName datafile_name = name.derive("iterator", datafile_filename);
    std::ifstream reader(datafile_name.get_full_path(), std::ios_base::binary);
    reader.seekg(byte_offset);

    for (int i = 0; i < count && !reader.eof(); i++) {
        std::string next;
        reader >> next;
        if (reader.eof()) {
            break;
        }
        out->emplace_back(next);
        file_offset += 1;
    }
    byte_offset = reader.tellg();
}

void OnDiskIterator::construct(
    const DatabaseName &location,
    const DatabaseName &backing_storage,
    int total_files
) {
    write_itermeta(location, 0, 0, total_files, backing_storage.get_filename());
}
