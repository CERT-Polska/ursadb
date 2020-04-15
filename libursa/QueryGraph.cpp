#include "QueryGraph.h"

#include <map>
#include <set>

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

template <typename T>
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

template <typename T>
class InorderGraphVisitor {
    std::vector<NodeId> ready_;
    const std::vector<QueryGraphNode> *nodes_;
    std::vector<NodeState<T>> state_;

   public:
    InorderGraphVisitor(std::vector<NodeId> ready,
                        const std::vector<QueryGraphNode> *nodes)
        : ready_(ready), nodes_(nodes), state_(nodes->size()) {
        for (size_t ndx = 0; ndx < nodes_->size(); ndx++) {
            for (NodeId target : (*nodes)[ndx].edges()) {
                state_.at(target.get()).add_predecessor();
            }
        }
    }

    bool empty() const { return ready_.empty(); }

    NodeId next() {
        NodeId nextid = ready_.back();
        ready_.pop_back();
        for (const auto &succ : (*nodes_)[nextid.get()].edges()) {
            state_[succ.get()].add_ready_predecessor(nextid);
            if (state_[succ.get()].ready()) {
                ready_.push_back(succ);
            }
        }

        return nextid;
    }

    std::vector<const T *> predecessor_states(NodeId id) {
        std::vector<const T *> states;
        const NodeState<T> &state = state_[id.get()];
        states.reserve(state.ready_predecessors().size());
        for (const auto &pred : state.ready_predecessors()) {
            states.push_back(&state_[pred.get()].state());
        }
        return std::move(states);
    }

    void set(NodeId id, T &&new_state) {
        state_[id.get()].set(std::move(new_state));
    }

    const T &getstate(NodeId id) const { return state_[id.get()].state(); }
};

QueryResult masked_or(std::vector<const QueryResult *> &&to_or,
                      QueryResult &&mask) {
    if (to_or.empty()) {
        return std::move(mask);
    }
    QueryResult result{QueryResult::empty()};
    for (const auto *query : to_or) {
        QueryResult alternative(std::vector<FileId>(query->vector()));
        alternative.do_and(mask);
        result.do_or(std::move(alternative));
    }
    return result;
}

QueryResult QueryGraph::run(const QueryFunc &oracle) const {
    if (sources_.empty()) {
        return QueryResult::everything();
    }
    QueryResult result{QueryResult::empty()};
    InorderGraphVisitor<QueryResult> visitor(sources_, &nodes_);
    while (!visitor.empty()) {
        NodeId next = visitor.next();
        visitor.set(next, std::move(masked_or(
                              std::move(visitor.predecessor_states(next)),
                              std::move(QueryResult(oracle(get(next).gram()))))));
        if (get(next).edges().size() == 0) {
            result.do_or(visitor.getstate(next));
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
            result.sources_ = new_sinks;
        }
        sinks = std::move(new_sinks);
    }

    return result;
}
