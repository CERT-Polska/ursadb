#pragma once

#include <string>
#include <vector>

#include "Core.h"
#include "QString.h"
#include "QueryResult.h"
#include "RawFile.h"
#include "Task.h"
#include "Utils.h"

struct IndexMergeHelper;

class OnDiskIndex {
    uint64_t index_size;
    std::string fname;
    fs::path fpath;
    RawFile ndxfile;
    IndexType ntype;

    static constexpr uint32_t VERSION = 6;
    std::vector<FileId> get_run(uint64_t ptr, uint64_t next_ptr) const;
    std::vector<FileId> query_primitive(TriGram trigram) const;
    std::pair<uint64_t, uint64_t> get_run_offsets(TriGram trigram) const;
    bool internal_expand(QString::const_iterator qit, uint8_t *out, size_t pos,
                         size_t comb_len, const TrigramGenerator &gen,
                         QueryResult &res) const;
    QueryResult expand_wildcards(const QString &qstr, size_t len,
                                 const TrigramGenerator &gen) const;

    static void on_disk_merge_core(const std::vector<IndexMergeHelper> &indexes,
                                   RawFile *out, TaskSpec *task);

   public:
    explicit OnDiskIndex(const std::string &fname);
    OnDiskIndex(const OnDiskIndex &) = delete;
    OnDiskIndex(OnDiskIndex &&) = default;

    const std::string &get_fname() const { return fname; }
    const fs::path &get_fpath() const { return fpath; }
    IndexType index_type() const { return ntype; }
    QueryResult query_str(const QString &str) const;
    unsigned long real_size() const;
    static void on_disk_merge(const fs::path &db_base, const std::string &fname,
                              IndexType merge_type,
                              const std::vector<IndexMergeHelper> &indexes,
                              TaskSpec *task);
    std::vector<uint64_t> read_run_offsets() const;
};

class OnDiskRun {
    uint64_t start_;
    uint64_t end_;

   public:
    OnDiskRun(uint64_t start, uint64_t end) : start_(start), end_(end) {}
    uint64_t end() const { return end_; }
    uint64_t start() const { return start_; }
    uint64_t size() const { return end_ - start_; }
};

struct IndexMergeHelper {
    const OnDiskIndex *index;
    uint32_t file_count;
    std::vector<uint64_t> run_offset_cache;

    IndexMergeHelper(const OnDiskIndex *index, uint32_t file_count,
                     std::vector<uint64_t> &&run_offset_cache)
        : index(index),
          file_count(file_count),
          run_offset_cache(run_offset_cache) {}
    IndexMergeHelper(const IndexMergeHelper &) = delete;
    IndexMergeHelper(IndexMergeHelper &&) = default;

    OnDiskRun run(uint32_t gram, size_t count = 1) const {
        return OnDiskRun(run_offset_cache[gram],
                         run_offset_cache[gram + count]);
    }
};
