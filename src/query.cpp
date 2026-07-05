#include "dagoba/query.hpp"
#include "dagoba/graph.hpp"

namespace dagoba {

// ---------------------------------------------------------------------------
// Concrete pipeline steps
// ---------------------------------------------------------------------------

namespace {

class VStep : public PipelineStep {
public:
    VStep(Graph& g, std::vector<VertexId> ids) : ids_(g.vertexIds(ids)) {}
    std::optional<Gremlin> next() override {
        if (idx_ >= ids_.size()) return std::nullopt;
        return Gremlin(ids_[idx_++]);
    }
private:
    std::vector<VertexId> ids_;
    std::size_t idx_ = 0;
};

// Shared logic for out()/in()/both(): pull a source gremlin from upstream,
// expand it into its neighbor ids, and drain that neighbor list one at a
// time before pulling upstream again.
class TraversalStep : public PipelineStep {
public:
    TraversalStep(PipelineStep& upstream, Graph& graph, std::string label)
        : upstream_(upstream), graph_(graph), label_(std::move(label)) {}

    std::optional<Gremlin> next() override {
        for (;;) {
            if (!pending_.empty()) {
                VertexId nextId = pending_.back();
                pending_.pop_back();
                return sourceGremlin_->fork(nextId);
            }
            auto src = upstream_.next();
            if (!src) return std::nullopt;
            sourceGremlin_ = src;
            pending_ = neighbors(src->currentVertex);
            // loop: either drain pending_ next iteration, or pull again if empty
        }
    }

protected:
    virtual std::vector<VertexId> neighbors(const VertexId& v) const = 0;

    PipelineStep& upstream_;
    Graph& graph_;
    std::string label_;

private:
    std::optional<Gremlin> sourceGremlin_;
    std::vector<VertexId> pending_;
};

class OutStep : public TraversalStep {
public:
    using TraversalStep::TraversalStep;
protected:
    std::vector<VertexId> neighbors(const VertexId& v) const override {
        return graph_.outVertices(v, label_);
    }
};

class InStep : public TraversalStep {
public:
    using TraversalStep::TraversalStep;
protected:
    std::vector<VertexId> neighbors(const VertexId& v) const override {
        return graph_.inVertices(v, label_);
    }
};

class BothStep : public TraversalStep {
public:
    using TraversalStep::TraversalStep;
protected:
    std::vector<VertexId> neighbors(const VertexId& v) const override {
        auto out = graph_.outVertices(v, label_);
        auto in = graph_.inVertices(v, label_);
        out.insert(out.end(), in.begin(), in.end());
        return out;
    }
};

class FilterStep : public PipelineStep {
public:
    FilterStep(PipelineStep& upstream, Graph& graph, VertexPredicate predicate)
        : upstream_(upstream), graph_(graph), predicate_(std::move(predicate)) {}

    std::optional<Gremlin> next() override {
        for (;;) {
            auto g = upstream_.next();
            if (!g) return std::nullopt;
            const Vertex* v = graph_.findVertex(g->currentVertex);
            if (v && predicate_(*v)) return g;
        }
    }
private:
    PipelineStep& upstream_;
    Graph& graph_;
    VertexPredicate predicate_;
};

class TakeStep : public PipelineStep {
public:
    TakeStep(PipelineStep& upstream, std::size_t n) : upstream_(upstream), n_(n) {}
    std::optional<Gremlin> next() override {
        if (taken_ >= n_) return std::nullopt;
        auto g = upstream_.next();
        if (g) ++taken_;
        return g;
    }
private:
    PipelineStep& upstream_;
    std::size_t n_;
    std::size_t taken_ = 0;
};

class UniqueStep : public PipelineStep {
public:
    explicit UniqueStep(PipelineStep& upstream) : upstream_(upstream) {}
    std::optional<Gremlin> next() override {
        for (;;) {
            auto g = upstream_.next();
            if (!g) return std::nullopt;
            if (seen_.insert(g->currentVertex).second) return g;
        }
    }
private:
    PipelineStep& upstream_;
    std::unordered_set<VertexId> seen_;
};

// as() is a pure passthrough at runtime; the tag itself is resolved to a
// path index at *build* time in Query::as (see below), which is enough
// for back() to rewind to it.
class PassthroughStep : public PipelineStep {
public:
    explicit PassthroughStep(PipelineStep& upstream) : upstream_(upstream) {}
    std::optional<Gremlin> next() override { return upstream_.next(); }
private:
    PipelineStep& upstream_;
};

class BackStep : public PipelineStep {
public:
    BackStep(PipelineStep& upstream, std::size_t hopIndex)
        : upstream_(upstream), hopIndex_(hopIndex) {}
    std::optional<Gremlin> next() override {
        auto g = upstream_.next();
        if (!g) return std::nullopt;
        if (hopIndex_ >= g->path.size()) return g; // tag never actually hopped; no-op
        Gremlin rewound = *g;
        rewound.path.resize(hopIndex_ + 1);
        rewound.currentVertex = rewound.path[hopIndex_];
        return rewound;
    }
private:
    PipelineStep& upstream_;
    std::size_t hopIndex_;
};

} // namespace

// ---------------------------------------------------------------------------
// Query builder methods
// ---------------------------------------------------------------------------

PipelineStep& Query::lastStep() { return *steps_.back(); }

Query& Query::v(std::vector<VertexId> ids) {
    steps_.push_back(std::make_unique<VStep>(graph_, std::move(ids)));
    ++hopCount_;
    return *this;
}

Query& Query::out(std::string label) {
    steps_.push_back(std::make_unique<OutStep>(lastStep(), graph_, std::move(label)));
    ++hopCount_;
    return *this;
}

Query& Query::in(std::string label) {
    steps_.push_back(std::make_unique<InStep>(lastStep(), graph_, std::move(label)));
    ++hopCount_;
    return *this;
}

Query& Query::both(std::string label) {
    steps_.push_back(std::make_unique<BothStep>(lastStep(), graph_, std::move(label)));
    ++hopCount_;
    return *this;
}

Query& Query::has(std::string key, std::string value) {
    return filter([key = std::move(key), value = std::move(value)](const Vertex& v) {
        auto it = v.properties.find(key);
        return it != v.properties.end() && it->second == value;
    });
}

Query& Query::filter(VertexPredicate predicate) {
    steps_.push_back(std::make_unique<FilterStep>(lastStep(), graph_, std::move(predicate)));
    return *this;
}

Query& Query::take(std::size_t n) {
    steps_.push_back(std::make_unique<TakeStep>(lastStep(), n));
    return *this;
}

Query& Query::unique() {
    steps_.push_back(std::make_unique<UniqueStep>(lastStep()));
    return *this;
}

Query& Query::as(std::string tag) {
    // Record the path index this tag refers to *before* pushing the
    // passthrough step: hopCount_ traversal steps have run, so the
    // gremlin's path currently has hopCount_ entries, and the "current"
    // one sits at index hopCount_ - 1.
    marks_[tag] = hopCount_ - 1;
    steps_.push_back(std::make_unique<PassthroughStep>(lastStep()));
    return *this;
}

Query& Query::back(const std::string& tag) {
    auto it = marks_.find(tag);
    std::size_t hopIndex = (it != marks_.end()) ? it->second : 0;
    steps_.push_back(std::make_unique<BackStep>(lastStep(), hopIndex));
    return *this;
}

std::vector<Gremlin> Query::run() {
    std::vector<Gremlin> results;
    if (steps_.empty()) return results;
    PipelineStep& final = lastStep();
    while (auto g = final.next()) {
        results.push_back(*g);
    }
    return results;
}

std::vector<VertexId> Query::ids() {
    std::vector<VertexId> out;
    for (auto& g : run()) out.push_back(g.currentVertex);
    return out;
}

} // namespace dagoba
