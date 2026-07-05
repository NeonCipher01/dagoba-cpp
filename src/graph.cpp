#include "dagoba/graph.hpp"
#include "dagoba/query.hpp"

namespace dagoba {

Vertex& Graph::addVertex(VertexId id, std::unordered_map<std::string, PropertyValue> props) {
    if (id.empty()) {
        id = "v" + std::to_string(autoIdCounter_++);
    }
    Vertex v;
    v.id = id;
    v.properties = std::move(props);
    auto [it, inserted] = vertices_.emplace(id, std::move(v));
    return it->second;
}

EdgeId Graph::nextEdgeId() {
    return "e" + std::to_string(edges_.size());
}

bool Graph::addEdge(const VertexId& outV, const VertexId& inV, std::string label,
                     std::unordered_map<std::string, PropertyValue> props) {
    auto* src = findVertex(outV);
    auto* dst = findVertex(inV);
    if (!src || !dst) return false;

    EdgeId id = nextEdgeId();
    Edge e;
    e.id = id;
    e.outV = outV;
    e.inV = inV;
    e.label = std::move(label);
    e.properties = std::move(props);

    edges_.emplace(id, std::move(e));
    src->outEdgeIds.push_back(id);
    dst->inEdgeIds.push_back(id);
    return true;
}

Vertex* Graph::findVertex(const VertexId& id) {
    auto it = vertices_.find(id);
    return it == vertices_.end() ? nullptr : &it->second;
}

const Vertex* Graph::findVertex(const VertexId& id) const {
    auto it = vertices_.find(id);
    return it == vertices_.end() ? nullptr : &it->second;
}

Edge* Graph::findEdge(const EdgeId& id) {
    auto it = edges_.find(id);
    return it == edges_.end() ? nullptr : &it->second;
}

const Edge* Graph::findEdge(const EdgeId& id) const {
    auto it = edges_.find(id);
    return it == edges_.end() ? nullptr : &it->second;
}

std::vector<VertexId> Graph::vertexIds(const std::vector<VertexId>& ids) const {
    if (!ids.empty()) {
        std::vector<VertexId> out;
        for (auto& id : ids) {
            if (vertices_.count(id)) out.push_back(id);
        }
        return out;
    }
    std::vector<VertexId> out;
    out.reserve(vertices_.size());
    for (auto& [id, v] : vertices_) out.push_back(id);
    return out;
}

std::vector<VertexId> Graph::outVertices(const VertexId& from, const std::string& label) const {
    std::vector<VertexId> result;
    auto* v = findVertex(from);
    if (!v) return result;
    for (auto& eid : v->outEdgeIds) {
        auto* e = findEdge(eid);
        if (e && (label.empty() || e->label == label)) result.push_back(e->inV);
    }
    return result;
}

std::vector<VertexId> Graph::inVertices(const VertexId& from, const std::string& label) const {
    std::vector<VertexId> result;
    auto* v = findVertex(from);
    if (!v) return result;
    for (auto& eid : v->inEdgeIds) {
        auto* e = findEdge(eid);
        if (e && (label.empty() || e->label == label)) result.push_back(e->outV);
    }
    return result;
}

Query Graph::v(std::vector<VertexId> ids) {
    Query q(*this);
    q.v(std::move(ids)); // mutates q in place; return value (Query&) unused here
    return q;             // named local of matching type -> moved out (NRVO/move ctor)
}

} // namespace dagoba
