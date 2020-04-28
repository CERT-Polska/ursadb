#include "Responses.h"

#include "Version.h"

Response Response::select(const std::vector<std::string> &files) {
    Response r("select");
    r.content["result"]["mode"] = "raw";
    r.content["result"]["files"] = files;
    return r;
}

Response Response::select_from_iterator(const std::vector<std::string> &files,
                                        uint64_t iterator_position,
                                        uint64_t total_files) {
    Response r("select");
    r.content["result"]["mode"] = "raw";
    r.content["result"]["files"] = files;
    r.content["result"]["iterator_position"] = iterator_position;
    r.content["result"]["total_files"] = total_files;
    return r;
}

Response Response::select_iterator(const std::string &iterator,
                                   uint64_t file_count) {
    Response r("select");
    r.content["result"]["mode"] = "iterator";
    r.content["result"]["file_count"] = file_count;
    r.content["result"]["iterator"] = iterator;
    return r;
}

Response Response::ok() {
    Response r("ok");
    r.content["result"]["status"] = "ok";
    return r;
}

Response Response::ping(const std::string &connection_id) {
    Response r("ping");
    r.content["result"]["status"] = "ok";
    r.content["result"]["connection_id"] = connection_id;
    r.content["result"]["ursadb_version"] = std::string(ursadb_version);
    return r;
}

Response Response::error(const std::string &message, bool retry) {
    Response r("error");
    r.content["error"]["retry"] = retry;
    r.content["error"]["message"] = message;
    return r;
}

Response Response::topology(const std::vector<DatasetEntry> &datasets) {
    Response r("topology");
    json datasets_json = std::map<std::string, std::string>{};
    for (auto &dataset : datasets) {
        std::vector<std::string> indexes;
        for (auto &index : dataset.indexes) {
            std::string type_name = get_index_type_name(index.type);
            json index_entry;
            index_entry["type"] = type_name;
            index_entry["size"] = index.size;
            datasets_json[dataset.id]["indexes"].push_back(index_entry);
        }
        datasets_json[dataset.id]["size"] = dataset.size;
        datasets_json[dataset.id]["file_count"] = dataset.file_count;
        datasets_json[dataset.id]["taints"] = dataset.taints;
    }
    r.content["result"]["datasets"] = datasets_json;
    return r;
}

Response Response::status(
    const std::unordered_map<uint64_t, TaskSpec *> &tasks) {
    Response r("status");
    std::vector<json> tasks_json;
    for (auto &[k, task] : tasks) {
        json task_json;
        task_json["id"] = task->id();
        task_json["connection_id"] = task->hex_conn_id();
        task_json["request"] = task->request_str();
        task_json["work_done"] = task->work_done();
        task_json["work_estimated"] = task->work_estimated();
        task_json["epoch_ms"] = task->epoch_ms();
        tasks_json.push_back(task_json);
    }
    r.content["result"]["tasks"] = tasks_json;
    r.content["result"]["ursadb_version"] = std::string(ursadb_version);

    return r;
}

Response Response::config(std::unordered_map<std::string, uint64_t> values) {
    Response r("config");
    r.content["result"]["keys"] = values;
    return r;
}

std::string Response::to_string() const { return content.dump(); }
