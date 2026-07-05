#pragma once

#include "types.hpp"
#include <optional>

namespace dagoba {

class Query; // fwd decl, defined in query.hpp

class Graph {
public:
    // Adds a vertex. If id is empty, one is auto-generated.
    Vertex& addVertex(VertexId id, std::unordered_map<std::string, PropertyValue> props = {});

    // Adds a directed edge outV -> inV. Both vertices must already exist.
    // Returns false if either endpoint is missing.
    bool addEdge(const VertexId& outV, const VertexId& inV, std::string label,
                 std::unordered_map<std::string, PropertyValue> props = {});

    Vertex* findVertex(const VertexId& id);
    const Vertex* findVertex(const VertexId& id) const;
    Edge* findEdge(const EdgeId& id);
    const Edge* findEdge(const EdgeId& id) const;

    // Returns vertices, filtered to `ids` if non-empty, else all vertices.
    std::vector<VertexId> vertexIds(const std::vector<VertexId>& ids = {}) const;

    // Traversal helpers used by pipetypes. `label` empty = any label.
    std::vector<VertexId> outVertices(const VertexId& from, const std::string& label = "") const;
    std::vector<VertexId> inVertices(const VertexId& from, const std::string& label = "") const;

    std::size_t vertexCount() const { return vertices_.size(); }
    std::size_t edgeCount() const { return edges_.size(); }

    // Entry point for building a query against this graph.
    Query v(std::vector<VertexId> ids = {});

private:
    std::unordered_map<VertexId, Vertex> vertices_;
    std::unordered_map<EdgeId, Edge> edges_;
    std::size_t autoIdCounter_ = 0;

    EdgeId nextEdgeId();
};

} // namespace dagoba
