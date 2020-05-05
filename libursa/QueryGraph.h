#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <string>

#include "QString.h"
#include "QueryResult.h"

// Strongly typed integer value, represents a node ID in a QueryGraph.
class NodeId {
    uint32_t id_;

   public:
    // Constructs a new nodeid object with assigned value.
    explicit NodeId(uint32_t id) : id_(id) {}

    // Compares two nodes by node id.
    bool operator<(const NodeId &rhs) const { return id_ < rhs.id_; }

    // Returns wrapped integer value.
    uint32_t get() const { return id_; }

    // Returns a NodeId shifted by `shift. Useful for moving getween graphs.
    NodeId shifted(size_t shift) { return NodeId(id_ + shift); }
};

// Edge, when necessary, is represented internally as a pair of nodes.
using Edge = std::pair<NodeId, NodeId>;

// Signature of the function used to query index by ngram.
using QueryFunc = std::function<QueryResult(uint64_t)>;

class QueryGraphNode {
    // Adjacency list for this node. Elements are indices in the parent
    // graph's `nodes` vector.
    std::vector<NodeId> edges_;

    // N-gram with implicit n. For example, 0x112233 represents {11 22 33}.
    // For obvious reasons, can't handle n-grams with n bigger than 8.
    uint64_t gram_;

    // How many predecessors are expected by this node. In almost all cases
    // this will be 1. Different values may show up for `min n of ...` queries.
    // This value is ignored for source vertexes (with no parents). Dual
    // operation is not supported for nodes with min_want_ != 1.
    uint32_t min_want_;

    // Is this a special epsilon node. Epsilon nodes are no-ops that accept
    // every file. They are sometimes added during graph operations.
    bool is_epsilon_;

    // Constructs an epsilon graph node - private to avoid accidental misuse.
    QueryGraphNode() : edges_(), gram_(0), min_want_(1), is_epsilon_(true) {}

   public:
    // Constructs a new graph node with assigned ngram value.
    explicit QueryGraphNode(uint64_t gram)
        : gram_(gram), min_want_(1), is_epsilon_(false) {}

    // Adds edge from this node to another one.
    void add_edge(NodeId to) { edges_.push_back(to); }

    // Returns true if this node is epsilon, false otherwise.
    bool is_epsilon() const { return is_epsilon_; }

    // Returns ngram assigned to this node, undefined if the node is epsilon.
    uint64_t gram() const {
        assert(!is_epsilon_);
        return gram_;
    }

    // Returns a list of edges adjacent to this one.
    const std::vector<NodeId> &edges() const { return edges_; }

    // Constructs an epsilon graph node.
    static QueryGraphNode epsilon() { return QueryGraphNode(); }

    // Constructs a node that accepts every file that appears in at least `n`
    // predecessor results.
    static QueryGraphNode min_of(uint32_t n) {
        auto result = epsilon();
        result.min_want_ = n;
        return result;
    }

    uint32_t min_want() const { return min_want_; }

    // Sink node is defined as a node with no outgoing edges.
    bool is_sink() const { return edges_.empty(); }

    // Shift this node by `offset` - useful when moving nodes between graphs.
    void shift(size_t offset) {
        for (auto &edge : edges_) {
            edge = edge.shifted(offset);
        }
    }
};

// A directed graph of n-grams. More advanced query representation. Enables
// more complex optimisations than the default tree form. Should be intepreted
// in a following way:
// "File matches the query, if there is at least path from source to sink".
// Sources are elements with no incoming edges. Sinks are alements with no
// outgoing edges. File matches graph node if it contains a n-gram that node
// represents.
// Some examples:
//
//  ABC  ->  BCD  ->  CDE
//
// Matches files with all of "ABC", "BCD" and "CDE".
//
//           BCX  ->  CXE  ->  XEF
//  ABC  -<                         >-  EFG
//           BCY  ->  CYE  ->  YEF
//
// Matches files with ABC, EFG and either (BCX, CXE, XEF) or (BCY, CYE, YEF).
// This is the most precise 3-gram decomposition of query "ABC(X|Y)EFG".
class QueryGraph {
    // List of nodes in this graph. Expected to be immutable after creation.
    std::vector<QueryGraphNode> nodes_;

    // List of source nodes - nodes that have no incoming edges.
    std::vector<NodeId> sources_;

    // Merges ngrams of two given nodes assuming they're adjacent in query.
    // For example, will merge ABC and BCD into ABCD.
    uint64_t combine(NodeId sourceid, NodeId targetid) const;

    // Adds an existing node to the query graph, and returns its id.
    NodeId make(QueryGraphNode node) {
        nodes_.emplace_back(std::move(node));
        return NodeId(nodes_.size() - 1);
    }

    // Adds a new node to the query graph, and returns its id.
    NodeId make_node(uint64_t gram) {
        nodes_.emplace_back(QueryGraphNode(gram));
        return NodeId(nodes_.size() - 1);
    }

    // Adds a new epsilon node to the query graph, and returns its id.
    NodeId make_epsilon() {
        nodes_.emplace_back(QueryGraphNode::epsilon());
        return NodeId(nodes_.size() - 1);
    }

    // Adds a new epsilon node to the query graph, and returns its id.
    NodeId make_min_of(uint32_t n) {
        nodes_.emplace_back(QueryGraphNode::min_of(n));
        return NodeId(nodes_.size() - 1);
    }

    // Gets a QueryGraphNode by its ID
    const QueryGraphNode &get(NodeId id) const { return nodes_[id.get()]; }

    // Gets a QueryGraphNode by its ID
    QueryGraphNode &get(NodeId id) { return nodes_[id.get()]; }

   public:
    // Constructs an empty query graph. By convention it matches everything.
    QueryGraph() = default;

    QueryGraph(const QueryGraph &other) = delete;
    QueryGraph(QueryGraph &&other) = default;
    QueryGraph &operator=(QueryGraph &&other) = default;

    // Constructs the edge-to-vertex dual of this graph, merging ngrams in
    // nodes in the expected way. This transformation maintains the query
    // graph invariant (of matching files). For example this will convert:
    //
    //               X
    // A  ->  B  -<     ->  C  ->  D
    //               Y
    // to:
    //
    //          BX  ->  XC
    //   AB -<              >-  CD
    //          BY  ->  YC
    //
    QueryGraph dual() const;

    // Gets all files matching the current graph, using a provided oracle.
    QueryResult run(const QueryFunc &oracle, QueryCounters *counters) const;

    // Returns a number of nodes in this query graph.
    uint32_t size() const { return nodes_.size(); }

    // Joins the second querygrah to this one. This is equivalent to AND.
    void and_(QueryGraph &&other);

    // Combines two graphs with the "OR" operation.
    void or_(QueryGraph &&other);

    // Combines multiple graphs with the "min of" operation.
    static QueryGraph min_of(uint32_t min_want,
                             std::vector<QueryGraph> &&others);

    // Converts the query to a naive graph of 1-grams.
    // For example, "ABCD" will be changed to `A -> B -> C -> D`.
    static QueryGraph from_qstring(const QString &qstr);
};
