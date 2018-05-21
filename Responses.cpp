#include "Responses.h"

Response Response::select(const std::vector<std::string> &files) {
    Response r("select");
    r.content["result"]["files"] = files;
    return r;
}

Response Response::ok() {
    Response r("ok");
    r.content["result"]["status"] = "ok";
    return r;
}

Response Response::error(const std::string &message) {
    Response r("error");
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
            datasets_json[dataset.id]["indexes"].push_back(index_entry);

        }
    }
    r.content["result"]["datasets"] = datasets_json;
    return r;
}

Response Response::status(const std::vector<TaskEntry> &tasks) {
    Response r("status");
    std::vector<json> tasks_json;
    for (auto &task : tasks) {
        json task_json;
        task_json["id"] = task.id;
        task_json["connection_id"] = task.connection_id;
        task_json["request"] = task.request;
        task_json["work_done"] = task.work_done;
        task_json["work_estimated"] = task.work_estimated;
        task_json["epoch_ms"] = task.epoch_ms;
        tasks_json.push_back(task_json);
    }
    r.content["result"]["tasks"] = tasks_json;
    return r;
}

std::string Response::to_string() const { return content.dump(); }
