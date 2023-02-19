#pragma once

#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core.h"
#include "OnDiskFileIndex.h"
#include "OnDiskIndex.h"
#include "Query.h"
#include "QueryResult.h"
#include "ResultWriter.h"
#include "Task.h"

// This class contains statistics about how common various ngrams are
// in the dataset. It is used during optimisation phase, to run queries on
// small datasets first (to speed up the overall process).
// There is a single ngram profile per database to save RAM. We could read
// this information from disk directly, but we want to avoid reading from
// disk when possible (after all this is the point of this class).
class NgramProfile {
   private:
    // The vectors here are run offsets from OnDiskIndex, i.e. ngram X spans
    // bytes from vector[X] to vector[X+1].
    std::map<IndexType, std::vector<uint64_t>> profiles;

   public:
    NgramProfile() : profiles() {}
    NgramProfile(std::map<IndexType, std::vector<uint64_t>> &&profiles)
        : profiles(std::move(profiles)) {}
    NgramProfile &operator=(NgramProfile &&other) = default;
    NgramProfile(NgramProfile &&other) = default;
    NgramProfile(const NgramProfile &other) = delete;

    // Returns the size in bytes of data for a given ngram.
    // Worth noting that the specific number is not important. What matters is
    // that - on average - more common ngrams will return bigger values.
    uint64_t size_in_bytes(PrimitiveQuery primitive) const;
};

// Represents a single dataset. Dataset is the smallest independent data
// component in mquery. For example, it's entirely possible to copy dataset
// from one server into another and expect it to work in the same way.
// Dataset has:
// - An unique name.
// - A set of 1 or more indexes (up to one of gram3, text4, wide8, hash4).
// - List of filenames contained in this dataset.
class OnDiskDataset {
    std::string name;
    fs::path db_base;
    std::optional<OnDiskFileIndex> files_index;
    std::vector<OnDiskIndex> indices;
    std::set<std::string> taints;

    bool is_taint_compatible(const OnDiskDataset &other) const {
        return taints == other.taints;
    }
    std::string get_file_name(FileId fid) const;
    QueryResult query(const Query &query, QueryCounters *counters) const;
    const OnDiskIndex &get_index_with_type(IndexType index_type) const;
    void drop_file(const std::string &fname) const;

   public:
    explicit OnDiskDataset(const fs::path &db_base, std::string fname);
    const std::string &get_name() const;
    fs::path get_base() const;
    const std::string &get_files_fname() const {
        return files_index->get_files_fname();
    }
    const std::string &get_fname_cache_fname() const {
        return files_index->get_cache_fname();
    }
    void toggle_taint(const std::string &taint);
    bool has_all_taints(const std::set<std::string> &taints) const;
    void execute(const Query &query, ResultWriter *out, QueryCounters *counters,
                 const NgramProfile &profile) const;
    uint64_t get_file_count() const { return files_index->get_file_count(); }
    void for_each_filename(std::function<void(const std::string &)> cb) const {
        files_index->for_each_filename(cb);
    }
    static void merge(const fs::path &db_base, const std::string &outname,
                      const std::vector<const OnDiskDataset *> &datasets,
                      TaskSpec *task);
    void save();
    void drop();
    std::string get_id() const;
    const std::vector<OnDiskIndex> &get_indexes() const { return indices; }
    const std::set<std::string> &get_taints() const { return taints; }
    static std::vector<const OnDiskDataset *> get_compact_candidates(
        const std::vector<const OnDiskDataset *> &datasets);
    NgramProfile generate_ngram_profile() const;

    // Returns vectors of compatible datasets. Datasets are called compatible
    // when they can be merged with each other - they have the same types and
    // the same taints.
    static std::vector<std::vector<const OnDiskDataset *>>
    get_compatible_datasets(const std::vector<const OnDiskDataset *> &datasets);
};
