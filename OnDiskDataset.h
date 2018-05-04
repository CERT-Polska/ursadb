#pragma once

#include <string>
#include <vector>

#include "Core.h"
#include "OnDiskIndex.h"
#include "Query.h"
#include "Task.h"

class OnDiskIndex;

class OnDiskDataset {
    std::string name;
    fs::path db_base;
    std::string files_fname;
    // set of indexed filenames ordered with respect to FileId
    std::vector<std::string> fnames;
    std::vector<OnDiskIndex> indices;

    const std::string &get_file_name(FileId fid) const;
    QueryResult query_str(const std::string &str) const;
    QueryResult internal_execute(const Query &query) const;
    const OnDiskIndex &get_index_with_type(IndexType index_type) const;
    void drop_file(const std::string &fname) const;

  public:
    explicit OnDiskDataset(const fs::path &db_base, const std::string &fname);
    const std::string &get_name() const;
    fs::path get_base() const;
    const std::vector<std::string> &indexed_files() const { return fnames; }
    void execute(const Query &query, std::vector<std::string> *out) const;
    static void
    merge(const fs::path &db_base, const std::string &outname,
          const std::vector<const OnDiskDataset *> &datasets, Task *task);
    void drop();
    std::string get_id() const;
    const std::vector<OnDiskIndex> &get_indexes() const { return indices; }

    template <typename T>
    static std::vector<T> get_compact_candidates(const std::vector<T> &datasets) {
        std::vector<T> out;

        struct DatasetScore {
            T ds;
            unsigned long size;

            DatasetScore(T ds, unsigned long size) : ds(ds), size(size) {}
        };

        struct compare_size {
            bool operator() (const DatasetScore& lhs, const DatasetScore& rhs) const {
                return lhs.size < rhs.size;
            }
        };

        if (datasets.size() < 2) {
            return out;
        }

        std::set<DatasetScore, compare_size> scores;

        for (auto *ds : datasets) {
            unsigned long dataset_size = 0;

            for (const auto &ndx : ds->get_indexes()) {
                dataset_size += fs::file_size(ds->get_base() / ndx.get_fname());
            }

            scores.emplace(ds, dataset_size);
        }

        auto it = scores.begin();
        auto &score1 = *it;
        auto &score2 = *(++it);

        if (score1.size * 2 > score2.size) {
            out.push_back(score1.ds);
            out.push_back(score2.ds);
        }

        return out;
    }
};
