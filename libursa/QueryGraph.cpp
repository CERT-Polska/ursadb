#include "QueryGraph.h"

#include <map>

QueryGraph QueryGraph::dual() const {
    QueryGraph result;

    std::map<Edge, NodeId> newnodes;
    for (size_t ndx = 0; ndx < nodes_.size(); ndx++) {
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

    std::vector<NodeId> sinks;
    for (const auto &token : qstr) {
        std::vector<NodeId> new_sinks;
        for (auto opt : token.possible_values()) {
            NodeId node = result.make_node(opt);
            for (NodeId left : sinks) {
                result.get(left).add_edge(node);
            }
            new_sinks.push_back(node);
        }
        sinks = std::move(new_sinks);
    }

    return result;
}
