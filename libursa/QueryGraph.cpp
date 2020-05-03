#include "QueryGraph.h"

#include <map>
#include <set>

QueryGraph QueryGraph::dual() const {
    QueryGraph result;

    // Create a mapping [old graph's edge] -> [new graph's node].
    std::map<Edge, NodeId> newnodes;
    for (size_t ndx = 0; ndx < nodes_.size(); ndx++) {
        NodeId source(ndx);
        for (NodeId target : get(source).edges()) {
            uint64_t gram = combine(source, target);
            newnodes.emplace(Edge{source, target}, result.make_node(gram));
        }
    }
    // Find sources in the new line graph.
    for (NodeId source : sources_) {
        for (NodeId target : get(source).edges()) {
            result.sources_.push_back(newnodes.at(Edge{source, target}));
        }
    }
    // Compute edges in the new graph (by combining neigh nodes).
    for (const auto &[edge, node] : newnodes) {
        auto &[from, to] = edge;
        for (const auto &target : get(to).edges()) {
            result.get(node).add_edge(newnodes.at(Edge{to, target}));
        }
    }

    return result;
}

// Helper class for InorderGraphVisitor. Remembers how many incoming edges
// are there for every node, and how many were already processed.
// Type T is a type of user-supplied state, and TM is its constructor.
template <typename T, T TM()>
class NodeState {
    std::vector<NodeId> ready_predecessors_;
    uint32_t total_predecessors_;
    T state_;

   public:
    // Constructs a new instance of NodeState with provided constructor.
    NodeState() : total_predecessors_{0}, state_{TM()} {}
    NodeState(const NodeState &other) = delete;
    NodeState(NodeState &&other) = default;

    // Gets a reference to internal state.
    const T &get() const { return state_; }

    // Moves the provided value into internal state.
    void set(T &&state) { state_ = std::move(state); }

    // Gets a vector of already processed predecessors.
    const std::vector<NodeId> &ready_predecessors() const {
        return ready_predecessors_;
    }

    // Used for counting incoming edges into the graphs node.
    void add_predecessor() { total_predecessors_++; }

    // Called when a predecessor was processed and becomes ready.
    void add_ready_predecessor(NodeId ready) {
        ready_predecessors_.push_back(ready);
    }

    // Node is assumed ready when all of its predecessors were processed.
    bool ready() const {
        return ready_predecessors_.size() >= total_predecessors_;
    }
};

// This class can be used to traverse QueryGraph DAG in topological order.
// It's guaranteed that during processing of every node, all it's parents
// were already processed.
template <typename T, T TM()>
class InorderGraphVisitor {
    std::vector<NodeId> ready_;
    const std::vector<QueryGraphNode> *nodes_;
    std::vector<NodeState<T, TM>> state_;

   public:
    // Creates a new instance of InorderGraphVisitor. `ready` vector should
    // contain verticles with no incoming edges (aka graph sources).
    InorderGraphVisitor(std::vector<NodeId> ready,
                        const std::vector<QueryGraphNode> *nodes)
        : ready_{std::move(ready)}, nodes_{nodes}, state_(nodes_->size()) {
        for (const auto &node : *nodes_) {
            for (NodeId target : node.edges()) {
                state_.at(target.get()).add_predecessor();
            }
        }
    }

    // True if traversal was finished and visit queue is empty.
    bool empty() const { return ready_.empty(); }

    // Returns id of next graph's node to process.
    NodeId next() {
        NodeId nextid = ready_.back();
        ready_.pop_back();
        for (const auto &succ : (*nodes_)[nextid.get()].edges()) {
            auto &successor = state_[succ.get()];
            successor.add_ready_predecessor(nextid);
            if (successor.ready()) {
                ready_.push_back(succ);
            }
        }
        return nextid;
    }

    // Returns a vector of states of all predecessors for a given node.
    std::vector<const T *> predecessor_states(NodeId id) {
        std::vector<const T *> states;
        const NodeState<T, TM> &state = state_[id.get()];
        states.reserve(state.ready_predecessors().size());
        for (const auto &pred : state.ready_predecessors()) {
            states.push_back(&state_[pred.get()].get());
        }
        return states;
    }

    // Sets a state for a node with a given id. New state is moved.
    void set(NodeId id, T &&new_state) {
        state_[id.get()].set(std::move(new_state));
    }

    // Gets a reference to state of a node with a given id.
    const T &state(NodeId id) const { return state_[id.get()].get(); }
};

// Executes a masked_or operation: `(A | B | C | ...) & mask`.
QueryResult masked_or(std::vector<const QueryResult *> &&to_or,
                      QueryResult &&mask, QueryStatistics *toupdate) {
    if (to_or.empty()) {
        // Empty or list means everything(). The only case when it happens
        // is for sources, when it makes sense to just return mask.
        return std::move(mask);
    }
    QueryResult result{QueryResult::empty()};
    for (const auto *query : to_or) {
        QueryResult alternative(query->clone());
        alternative.do_and(mask, toupdate);
        result.do_or(std::move(alternative), toupdate);
    }
    return result;
}

// Executes a masked minof operation: `min n of (A, B, C, ...) & mask`.
QueryResult masked_min_of(uint32_t min_want,
                          std::vector<const QueryResult *> &&sources,
                          QueryResult &&mask, QueryStatistics *toupdate) {
    if (sources.empty()) {
        // Empty or list means everything(). The only case when it happens
        // is for sources, when it makes sense to just return mask.
        return std::move(mask);
    }
    // The query is equivalent to AND, just run ANDs in a loop.
    if (min_want == sources.size()) {
        for (const auto *source : sources) {
            mask.do_and(*source, toupdate);
        }
        return std::move(mask);
    }
    // Don't pay the price of generic solution in the common case`.
    if (min_want == 1) {
        return masked_or(std::move(sources), std::move(mask), toupdate);
    }
    // Do a real `min ... of` operation.
    QueryResult result{QueryResult::do_min_of(min_want, sources, toupdate)};
    result.do_and(std::move(mask), toupdate);
    return result;
}

uint64_t QueryGraph::combine(NodeId sourceid, NodeId targetid) const {
    const auto &source = get(sourceid);
    const auto &target = get(targetid);
    if (source.is_epsilon() || target.is_epsilon()) {
        // It's not that it's hard or slow to combine epsilons. We just
        // don't expect this, so this may warrant investigation.
        throw std::runtime_error("Unexpected epsilon combine op.");
    }
    if (source.min_want() != 1 || target.min_want() != 1) {
        // Dual() algorithm won't work correctly for min_of nodes.
        throw std::runtime_error("Can't combine min_of nodes.");
    }
    return (source.gram() << 8) + (target.gram() & 0xFF);
}

QueryResult QueryGraph::run(const QueryFunc &oracle) const {
    if (nodes_.empty()) {
        // The graph has no nodes. By convention this means that it maches
        // every file (because it doesn't exclude any file).
        return QueryResult::everything();
    }
    QueryResult result{QueryResult::empty()};
    InorderGraphVisitor<QueryResult, QueryResult::everything> visitor(sources_,
                                                                      &nodes_);
    QueryStatistics stats;
    while (!visitor.empty()) {
        // New state is: (union of all possible predecessors) & oracle(id)
        NodeId id{visitor.next()};
        auto predecessors = visitor.predecessor_states(id);
        // Optimisation: when all predecessors are empty, there's no point in
        // reading current ngram to `and` with them.
        bool sources_empty = true;
        for (const auto &pred : predecessors) {
            if (!pred->is_empty()) {
                sources_empty = false;
            }
        }
        if (sources_empty && !predecessors.empty()) {
            visitor.set(id, QueryResult::empty());
        } else {
            // Do a query, or take everything for epsilon.
            QueryResult next = {get(id).is_epsilon()
                                    ? QueryResult::everything()
                                    : QueryResult(oracle(get(id).gram()))};
            visitor.set(id, std::move(masked_min_of(get(id).min_want(),
                                                    std::move(predecessors),
                                                    std::move(next), &stats)));
        }
        if (get(id).edges().empty()) {
            result.do_or(visitor.state(id), &stats);
        }
    }
    result.set_stats(stats);
    return result;
}

QueryGraph QueryGraph::from_qstring(const QString &qstr) {
    QueryGraph result;

    // Create a node for every possible value of every token, and attach it
    // to nodes from the previous iteration.
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

// This method will join two graphs, effectively ANDing them. The way it works
// is best understood by picture, for example:
//
// Graph A:                  Graph B:
//               C           F  ->  H
// A  ->  B  -<                         >-  J
//               D           G  ->  I
//
// To join these graphs, we introduce imaginary epsilon node, that will glue
// them together. Epsilon nodes accept every file, so the behaviour will not
// change:
//
//               C           F  ->  H
// A  ->  B  -<     >-(E)-<            >-  J
//               D           G  ->  I
//
void QueryGraph::and_(QueryGraph &&other) {
    // 1. Special case - if the current graph is empty, just replace it with
    // the other one.
    if (nodes_.empty()) {
        *this = std::move(other);
        return;
    }

    // 2. Create the epsilon node that will serve as a bridge.
    NodeId epsilon{make_epsilon()};

    // 3. Append it to all sinks of this graph (C and D above).
    std::vector<NodeId> sinks;
    for (auto &node : nodes_) {
        if (&node != &nodes_.back() && node.is_sink()) {
            node.add_edge(epsilon);
        }
    }

    // 4. Paste the second graph into this one, updating IDs along the way.
    size_t shift = nodes_.size();
    for (auto &node : other.nodes_) {
        node.shift(shift);
        nodes_.emplace_back(std::move(node));
    }

    // 5. Append sources of the second graph (F and G above).
    for (auto &source : other.sources_) {
        get(epsilon).add_edge(source.shifted(shift));
    }
}

// This method will combine two graphs with the OR operation. It's very easy
// to implement because of the way we store graphs - we just merge nodes.
// For example, to merge:
//
// Graph A:              Graph B:
//
// A  ->  B  ->  C        D  ->  E  ->  F
//
// We just need to merge them (no shared nodes are necessary):
//
//            A  ->  B  ->  C
//            D  ->  E  ->  F
//
void QueryGraph::or_(QueryGraph &&other) {
    // 1. Special case for empty graph.
    if (nodes_.empty() || other.nodes_.empty()) {
        nodes_.clear();
        sources_.clear();
        return;
    }

    // 2. Paste the second graph into this one, updating IDs along the way.
    size_t shift = nodes_.size();
    for (auto &node : other.nodes_) {
        node.shift(shift);
        nodes_.emplace_back(std::move(node));
    }

    // 3. Append sources of the second graph (F and G above).
    for (auto &source : other.sources_) {
        sources_.emplace_back(source.shifted(shift));
    }
}

// This method will combine multiple graphs with the min ... of operation.
// It's pretty tricky to implement, because we need to add epsilon nodes
// to every subquery (for proper counting) and then add one more node (min of).
// For example, to do min 2 of (A, B, C):
//
// Graph A:                  Graph B:                Graph C:
//
//               C           F  ->  H
// A  ->  B  -<                         >-  J        K  ->  L  ->  M
//               D           G  ->  I
//
// We need to add epsilon node, and then merge them all with a special node:
//
//               C
// A  ->  B  -<     >- (E)
//               D        v
// F  ->  H                v
//           >-  J  -----> N(2)
// G  ->  I                ^
//                        ^
// K  ->  L  ->  M  -----/
//
QueryGraph QueryGraph::min_of(uint32_t min_want,
                              std::vector<QueryGraph> &&others) {
    // 1. Create the epsilon node that will serve as a bridge.
    QueryGraph result;
    NodeId bridge{result.make_min_of(min_want)};

    // 2. Merge all graphs into one large one:
    for (auto &graph : others) {
        // 2.1 Paste all nodes from this graph
        size_t shift = result.nodes_.size();
        std::vector<NodeId> sinks;
        for (auto &node : graph.nodes_) {
            node.shift(shift);
            NodeId id = result.make(std::move(node));

            if (result.get(id).is_sink()) {
                sinks.emplace_back(id);
            }
        }
        // 2.2 If there are 0 sinks, or more than one sink, add epsilon.
        if (sinks.size() != 1) {
            NodeId epsilon = result.make_epsilon();
            for (const auto &subsink : sinks) {
                result.get(subsink).add_edge(epsilon);
            }
            result.get(epsilon).add_edge(bridge);
            if (sinks.empty()) {
                result.sources_.emplace_back(epsilon);
            }
        } else {
            result.get(sinks.front()).add_edge(bridge);
        }
        // 2.3 Update sources of the resulting graph
        for (auto &source : graph.sources_) {
            result.sources_.emplace_back(source.shifted(shift));
        }
    }
    return result;
}
