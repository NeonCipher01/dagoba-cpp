#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace dagoba {

using VertexId = std::string;
using EdgeId = std::string;
using PropertyValue = std::string; // kept simple on purpose; see README for extension notes

// ---------------------------------------------------------------------------
// Vertex / Edge
// ---------------------------------------------------------------------------

struct Vertex {
    VertexId id;
    std::unordered_map<std::string, PropertyValue> properties;

    // Index-free adjacency: each vertex knows its own edges directly,
    // instead of scanning a global edge table. This is the core idea
    // that makes a graph DB fast at traversal vs. a relational join.
    std::vector<EdgeId> outEdgeIds;
    std::vector<EdgeId> inEdgeIds;
};

struct Edge {
    EdgeId id;
    VertexId outV; // source
    VertexId inV;  // target
    std::string label;
    std::unordered_map<std::string, PropertyValue> properties;
};

// ---------------------------------------------------------------------------
// Gremlin: the traveler that walks the graph as the pipeline executes.
// It's intentionally lightweight - it just remembers where it is and
// the vertices it passed through (for step-back / path queries).
// ---------------------------------------------------------------------------

struct Gremlin {
    VertexId currentVertex;
    std::vector<VertexId> path; // history of vertices visited, oldest first

    explicit Gremlin(VertexId v) : currentVertex(std::move(v)) {
        path.push_back(currentVertex);
    }

    Gremlin fork(const VertexId& nextVertex) const {
        Gremlin g(*this);
        g.currentVertex = nextVertex;
        g.path.push_back(nextVertex);
        return g;
    }
};

// ---------------------------------------------------------------------------
// Predicate type used by Query::filter() (see query.hpp)
// ---------------------------------------------------------------------------

using VertexPredicate = std::function<bool(const Vertex&)>;

} // namespace dagoba
