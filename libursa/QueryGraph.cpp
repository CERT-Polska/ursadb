#include "QueryGraph.h"

#include <map>
#include <set>

#include "spdlog/spdlog.h"

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
    for (NodeId source : sources_) {
        for (NodeId target : get(source).edges()) {
            result.sources_.push_back(newnodes.at(Edge{source, target}));
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

class NodeState {
    std::vector<NodeId> ready_predecessors_;
    uint32_t total_predecessors_;
    QueryResult state_;

   public:
    NodeState() : state_(QueryResult::everything()), total_predecessors_(0) {}

    const QueryResult &state() const { return state_; }

    const std::vector<NodeId> &ready_predecessors() const {
        return ready_predecessors_;
    }

    void set(QueryResult &&state) { state_ = std::move(state); }

    void add_predecessor() { total_predecessors_++; }

    void add_ready_predecessor(NodeId ready) {
        ready_predecessors_.push_back(ready);
    }

    bool ready() const {
        return ready_predecessors_.size() >= total_predecessors_;
    }
};

QueryResult masked_or(std::vector<const QueryResult *> *to_or,
                      QueryResult &&mask) {
    if (to_or->empty()) {
        return std::move(mask);
    }
    QueryResult result{QueryResult::empty()};
    for (auto query : *to_or) {
        // TODO(msm): we should do everything in parallel here.
        QueryResult alternative{query->vector()};
        alternative.do_and(mask);
        result.do_or(std::move(alternative));
    }
    return result;
}

QueryResult QueryGraph::run(const QueryFunc &oracle) const {
    if (sources_.empty()) {
        return QueryResult::everything();
    }
    std::vector<NodeId> ready = sources_;
    QueryResult result{QueryResult::empty()};
    std::vector<NodeState> states(nodes_.size());
    for (size_t ndx = 0; ndx < nodes_.size(); ndx++) {
        for (NodeId target : get(NodeId(ndx)).edges()) {
            states.at(target.get()).add_predecessor();
        }
    }
    while (ready.size()) {
        NodeId nextid = ready.back();
        ready.pop_back();
        NodeState &next = states[nextid.get()];
        std::vector<const QueryResult *> pred_states;
        pred_states.reserve(next.ready_predecessors().size());
        for (const auto &pred : next.ready_predecessors()) {
            pred_states.push_back(&states[pred.get()].state());
        }
        QueryResult next_state{oracle(get(nextid).gram())};
        next.set(std::move(masked_or(&pred_states, std::move(next_state))));
        for (const auto &succ : get(nextid).edges()) {
            states[succ.get()].add_ready_predecessor(nextid);
            if (states[succ.get()].ready()) {
                ready.push_back(succ);
            }
        }
        if (get(nextid).edges().size() == 0) {
            result.do_or(next.state());
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
        if (result.sources_.empty()) {
            spdlog::info("Setting up sources with {} nodes", new_sinks.size());
            result.sources_ = new_sinks;
        }
        sinks = std::move(new_sinks);
    }

    return result;
}
