#include "OnDiskFileIndex.h"

#include <fstream>

#include "spdlog/spdlog.h"

void OnDiskFileIndex::generate_namecache_file() {
    spdlog::debug("Namecache {} not found, generating", cache_fname);

    std::ofstream of;
    of.exceptions(std::ofstream::badbit);
    of.open(db_base / cache_fname, std::ofstream::binary);

    uint64_t offset = 0;
    uint64_t count = 0;
    for_each_filename([&of, &offset, &count](const std::string &fname) {
        of.write(reinterpret_cast<char *>(&offset), sizeof(offset));
        offset += fname.size() + 1;
        count += 1;
    });
    file_count = count;
    of.write(reinterpret_cast<char *>(&offset), sizeof(offset));
    of.flush();
    spdlog::info("SAVE: {}", cache_fname);
}

OnDiskFileIndex::OnDiskFileIndex(const fs::path &db_base,
                                 const std::string &files_fname,
                                 const std::string &cache_fname)
    : db_base(db_base),
      files_fname(files_fname),
      cache_fname(cache_fname),
      files_file(db_base / files_fname) {  // <- cool race condition here
    files_file.fadvise(POSIX_FADV_RANDOM);
    if (fs::exists(db_base / cache_fname)) {
        file_count = fs::file_size(db_base / cache_fname) / 8 - 1;
    } else {
        generate_namecache_file();
    }
    cache_file.emplace(db_base / cache_fname);
}

std::string OnDiskFileIndex::get_file_name(FileId fid) const {
    uint64_t ptrs[2];
    cache_file->pread(&ptrs, sizeof(ptrs), fid * sizeof(uint64_t));

    uint64_t filename_start = ptrs[0];
    uint64_t filename_end = ptrs[1] - 1;
    uint64_t filename_length = filename_end - filename_start;

    std::string filename(filename_length, '\x00');
    files_file.pread(filename.data(), filename_length, filename_start);
    return filename;
}

void OnDiskFileIndex::for_each_filename(
    const std::function<void(const std::string &)> &cb) const {
    std::string filename;
    std::ifstream inf(db_base / files_fname, std::ifstream::binary);

    while (!inf.eof()) {
        std::getline(inf, filename);

        if (!filename.empty()) {
            cb(filename);
        }
    }
}
