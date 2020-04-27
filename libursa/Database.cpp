#include "Database.h"

#include <experimental/filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "Core.h"
#include "ExclusiveFile.h"
#include "Json.h"
#include "Version.h"
#include "spdlog/spdlog.h"

Database::Database(const std::string &fname, bool initialize)
    : last_task_id(0), tasks() {
    db_name = fs::path(fname).filename();
    db_base = fs::path(fname).parent_path();

    if (initialize) {
        load_from_disk();
    }
}

Database::Database(const std::string &fname) : Database(fname, true) {}

OnDiskDataset *Database::find_working_dataset(const std::string &name) {
    for (auto ds : working_datasets) {
        if (ds->get_id() == name) {
            return ds;
        }
    }
    return nullptr;
}

void Database::load_from_disk() {
    std::ifstream db_file(db_base / db_name, std::ifstream::binary);

    if (db_file.fail()) {
        throw std::runtime_error("Failed to open database file");
    }

    json db_json;

    try {
        db_file >> db_json;
    } catch (json::parse_error &e) {
        throw std::runtime_error("Failed to parse JSON");
    }

    for (const std::string &dataset_fname : db_json["datasets"]) {
        load_dataset(dataset_fname);
    }

    for (const auto &iterator : db_json["iterators"].items()) {
        DatabaseName name(db_base, "iterator", iterator.key(),
                          iterator.value());
        load_iterator(name);
    }
}

void Database::create(const std::string &fname) {
    ExclusiveFile lock(fname);
    if (!lock.is_ok()) {
        throw std::runtime_error("File already exists");
    }
    Database empty(fname, false);
    empty.save();
}

uint64_t Database::allocate_task_id() { return ++last_task_id; }

Task *Database::allocate_task(const std::string &request,
                              const std::string &conn_id) {
    while (true) {
        uint64_t task_id = allocate_task_id();
        auto timestamp = std::chrono::steady_clock::now().time_since_epoch();
        uint64_t epoch_ms = get_milli_timestamp();
        if (tasks.count(task_id) == 0) {
            return tasks
                .emplace(task_id, std::make_unique<Task>(Task(
                                      task_id, epoch_ms, request, conn_id)))
                .first->second.get();
        }
    }
}

Task *Database::allocate_task() { return allocate_task("N/A", "N/A"); }

void Database::save() {
    std::string tmp_db_name =
        "tmp-" + random_hex_string(8) + "-" + db_name.string();
    std::ofstream db_file;
    db_file.exceptions(std::ofstream::badbit);
    db_file.open(db_base / tmp_db_name, std::ofstream::binary);

    json db_json;
    db_json["config"] = std::unordered_map<std::string, std::string>();

    std::vector<std::string> dataset_names;
    for (const auto *ds : working_datasets) {
        dataset_names.push_back(ds->get_name());
    }
    db_json["datasets"] = dataset_names;

    json iterators_json = std::unordered_map<std::string, std::string>();
    for (const auto &it : iterators) {
        iterators_json[it.first] = it.second.get_name().get_filename();
    }
    db_json["iterators"] = iterators_json;
    db_json["version"] = std::string(ursadb_format_version);

    db_file << std::setw(4) << db_json << std::endl;
    db_file.flush();
    db_file.close();

    fs::rename(db_base / tmp_db_name, db_base / db_name);
}

void Database::load_iterator(const DatabaseName &name) {
    iterators.emplace(name.get_id(), OnDiskIterator(name));
    spdlog::info("Loaded new iterator {}", name.get_filename());
}

void Database::load_dataset(const std::string &ds) {
    loaded_datasets.push_back(std::make_unique<OnDiskDataset>(db_base, ds));
    working_datasets.push_back(loaded_datasets.back().get());
    spdlog::info("Loaded new dataset {}", ds);
}

void Database::update_iterator(const DatabaseName &name, uint64_t byte_offset,
                               uint64_t file_offset) {
    auto it = iterators.find(name.get_id());
    if (it == iterators.end()) {
        spdlog::warn("Can't update invalid iterator {}", name.get_id());
        return;
    }
    OnDiskIterator &iter = it->second;
    if (file_offset >= iter.get_total_files()) {
        iter.drop();
        iterators.erase(name.get_id());
    } else {
        iter.update_offset(byte_offset, file_offset);
        iter.save();
    }
}

void Database::drop_dataset(const std::string &dsname) {
    for (auto it = working_datasets.begin(); it != working_datasets.end();) {
        if ((*it)->get_name() == dsname) {
            it = working_datasets.erase(it);
            spdlog::info("Drop dataset {}", dsname);
        } else {
            ++it;
        }
    }
}

void Database::destroy_dataset(const std::string &dsname) {
    for (auto it = loaded_datasets.begin(); it != loaded_datasets.end();) {
        if ((*it)->get_name() == dsname) {
            spdlog::info("Destroying dataset {}", dsname);
            (*it)->drop();
            it = loaded_datasets.erase(it);
        } else {
            ++it;
        }
    }
}

void Database::collect_garbage(
    std::set<DatabaseSnapshot *> &working_snapshots) {
    std::set<std::string> required_datasets;

    for (const auto *ds : working_sets()) {
        required_datasets.insert(ds->get_name());
    }

    for (const auto *snap : working_snapshots) {
        for (const auto *ds : snap->get_datasets()) {
            required_datasets.insert(ds->get_name());
        }
    }

    std::vector<std::string> drop_list;
    for (const auto &set : loaded_sets()) {
        if (required_datasets.count(set->get_name()) == 0) {
            // set is loaded but not required
            drop_list.push_back(set->get_name());
        }
    }

    for (const auto &ds : drop_list) {
        destroy_dataset(ds);
    }
}

void Database::commit_task(uint64_t task_id) {
    Task *task = get_task(task_id);

    for (const auto &change : task->changes) {
        spdlog::info("Change: {} {} ({})", db_change_to_string(change.type),
                     change.obj_name, change.parameter);
        if (change.type == DbChangeType::Insert) {
            load_dataset(change.obj_name);
        } else if (change.type == DbChangeType::Drop) {
            drop_dataset(change.obj_name);
        } else if (change.type == DbChangeType::Reload) {
            drop_dataset(change.obj_name);
            load_dataset(change.obj_name);
        } else if (change.type == DbChangeType::ToggleTaint) {
            OnDiskDataset *ds = find_working_dataset(change.obj_name);
            if (!ds) {
                return;  // suspicious, but maybe delayed task
            }
            ds->toggle_taint(change.parameter);
            ds->save();
        } else if (change.type == DbChangeType::NewIterator) {
            DatabaseName itname = DatabaseName::parse(db_base, change.obj_name);
            load_iterator(itname);
        } else if (change.type == DbChangeType::UpdateIterator) {
            DatabaseName itname = DatabaseName::parse(db_base, change.obj_name);
            std::string param = change.parameter;
            size_t split_loc = param.find(':');
            if (split_loc == std::string::npos) {
                throw std::runtime_error("Invalid iterator update parameter");
            }
            uint64_t new_bytes = std::atoi(param.substr(0, split_loc).c_str());
            uint64_t new_files = std::atoi(param.substr(split_loc + 1).c_str());
            update_iterator(itname, new_bytes, new_files);
        } else {
            throw std::runtime_error("unknown change type requested");
        }
    }

    if (!task->changes.empty()) {
        save();
    }

    erase_task(task_id);
}

Task *Database::get_task(uint64_t task_id) { return tasks.at(task_id).get(); }

void Database::erase_task(uint64_t task_id) { tasks.erase(task_id); }

DatabaseSnapshot Database::snapshot() {
    std::vector<const OnDiskDataset *> cds;

    for (const auto *d : working_datasets) {
        cds.push_back(d);
    }

    return DatabaseSnapshot(db_name, db_base, iterators, cds, tasks);
}
