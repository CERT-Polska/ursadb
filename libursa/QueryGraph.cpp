#include "QueryGraph.h"

#include <map>

QueryGraph QueryGraph::dual() const {
    QueryGraph result;

    std::map<Edge, NodeId> newnodes;
    for (int ndx = 0; ndx < nodes_.size(); ndx++) {
        NodeId source(ndx);
        for (NodeId target : get(source).edges()) {
            uint32_t gram = combine(source, target);
            newnodes.emplace(Edge{source, target}, result.make_node(gram));
        }
    }
    for (const auto &[edge, node] : newnodes) {
        auto &[from, to] = edge;
        for (const auto &target : get(to).edges()) {
            result.get(node).add_edge(newnodes.at(Edge{to, target}));
        }
    }

    return result;
}

QueryGraph QueryGraph::from_qstring(const QString &qstr) {
    QueryGraph result;

    if (qstr.empty()) {
        return result;
    }

    for (uint8_t opt : qstr[0].possible_values()) {
        result.sources_.push_back(result.make_node(opt));
    }

    std::vector<NodeId> sinks(result.sources_);
    for (int i = 1; i < qstr.size(); i++) {
        std::vector<NodeId> new_sinks;
        for (auto opt : qstr[i].possible_values()) {
            NodeId node = result.make_node(opt);
            for (NodeId left : sinks) {
                result.get(left).add_edge(node);
            }
            new_sinks.push_back(node);
        }
        sinks = new_sinks;
    }

    return result;
}
