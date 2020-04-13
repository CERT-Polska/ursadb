#pragma once

#include <cstdint>
#include <string>

#include "Core.h"

// Strongly typed integer value, represents a node ID in a QueryGraph.
class NodeId {
    uint32_t id_;

   public:
    NodeId(uint32_t id) : id_(id) {}

    bool operator<(const NodeId &rhs) const { return id_ < rhs.id_; }

    uint32_t get() const { return id_; }
};

using Edge = std::pair<NodeId, NodeId>;

class QueryGraphNode {
    // N-gram with implicit n. For example, 0x112233 represents {11 22 33}.
    // For obvious reasons, can't handle n-grams with n bigger than 4.
    uint32_t gram_;

    // Adjacency list for this node. Elements are indices in the parent
    // graph's `nodes` vector.
    std::vector<NodeId> edges_;

   public:
    QueryGraphNode(uint32_t gram) : gram_(gram) {}

    void add_edge(NodeId to) { edges_.push_back(to); }

    uint32_t gram() const { return gram_; }

    const std::vector<NodeId> &edges() const { return edges_; }

    // Sink node is defined as a node with no outgoing edges.
    bool is_sink() const { return edges_.empty(); }
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

    QueryGraph() {}
    QueryGraph(const QueryGraph &other) = delete;
    QueryGraph(QueryGraph &&other) = default;

    // Merges ngrams of two given nodes assuming they're adjacent in query.
    // For example, will merge ABC and BCD into ABCD.
    uint32_t combine(NodeId source, NodeId target) const {
        return (get(source).gram() << 8) + (get(target).gram() & 0xFF);
    }

    // Adds a new node to the query graph, and returns its id.
    NodeId make_node(uint32_t gram) {
        nodes_.emplace_back(QueryGraphNode(gram));
        return NodeId(nodes_.size() - 1);
    }

    const QueryGraphNode &get(NodeId id) const { return nodes_[id.get()]; }

    QueryGraphNode &get(NodeId id) { return nodes_[id.get()]; }

   public:
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
    QueryGraph dual() const;

    // Converts the query to a naive graph of 1-grams.
    // For example, "ABCD" will be changed to `A -> B -> C -> D`.
    static QueryGraph from_qstring(const QString &qstr);
};
