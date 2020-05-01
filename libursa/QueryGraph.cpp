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
                      QueryResult &&mask) {
    if (to_or.empty()) {
        // Empty or list means everything(). The only case when it happens
        // is for sources, when it makes sense to just return mask.
        return std::move(mask);
    } else if (to_or.size() == 1) {
        // In a very common case of a single predecessor, just do explicit and.
        mask.do_and(*to_or[0]);
        return std::move(mask);
    }
    QueryResult result{QueryResult::empty()};
    for (const auto *query : to_or) {
        QueryResult alternative(query->clone());
        alternative.do_and(mask);
        result.do_or(std::move(alternative));
    }
    return result;
}

uint64_t QueryGraph::combine(NodeId source, NodeId target) const {
    if (get(source).is_epsilon() || get(target).is_epsilon()) {
        // It's not that it's hard or slow to combine epsilons. We just
        // don't expect this, so this may warrant investigation.
        throw new std::runtime_error("Unexpected epsilon combine op");
    }
    return (get(source).gram() << 8) + (get(target).gram() & 0xFF);
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
    while (!visitor.empty()) {
        // New state is: (union of all possible predecessors) & oracle(id)
        NodeId id{visitor.next()};
        QueryResult next = {get(id).is_epsilon()
                                ? QueryResult::everything()
                                : QueryResult(oracle(get(id).gram()))};
        visitor.set(
            id, std::move(masked_or(std::move(visitor.predecessor_states(id)),
                                    std::move(next))));
        if (get(id).edges().size() == 0) {
            result.do_or(visitor.state(id));
        }
    }
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
// Graph A:              Graph B:
//               C           F  --  H
// A  ->  B  -<                         >-  J
//               D           G  --  I
//
// To join these graphs, we introduce imaginary epsilon node, that will glue
// them together. Epsilon nodes accept every file, so the behaviour will not
// change:
//
//               C           F  --  H
// A  ->  B  -<     >-(E)-<            >-  J
//               D           G  --  I
//
void QueryGraph::join(QueryGraph &&other) {
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
