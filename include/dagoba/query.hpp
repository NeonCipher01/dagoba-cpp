#pragma once

#include "types.hpp"
#include <memory>
#include <optional>
#include <unordered_set>

namespace dagoba {

class Graph;

// ---------------------------------------------------------------------------
// PipelineStep: each query operation (v, out, filter, take, ...) is a small
// pull-based iterator. Calling next() either produces the next Gremlin or
// std::nullopt once exhausted. Traversal steps pull from their upstream step
// only when they've run out of local work - this is what makes the whole
// pipeline lazy (e.g. take(3) will never force more than 3 results' worth
// of upstream traversal, even over a huge/cyclic graph).
// ---------------------------------------------------------------------------

class PipelineStep {
public:
    virtual ~PipelineStep() = default;
    virtual std::optional<Gremlin> next() = 0;
};

// ---------------------------------------------------------------------------
// Query: builds a pipeline against a Graph and evaluates it lazily.
// Steps are owned by the Query so references between them stay valid
// even as more steps are appended (unique_ptr indirection => stable addresses).
// ---------------------------------------------------------------------------

class Query {
public:
    explicit Query(Graph& g) : graph_(g) {}

    // Query owns unique_ptrs to its pipeline steps, so it's move-only -
    // copying a query pipeline mid-flight isn't a meaningful operation anyway.
    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;
    Query(Query&&) = default;
    Query& operator=(Query&&) = delete; // graph_ is a reference; can't be reassigned

    Query& v(std::vector<VertexId> ids = {});
    Query& out(std::string label = "");
    Query& in(std::string label = "");
    Query& both(std::string label = "");
    Query& has(std::string key, std::string value);     // property-equality filter
    Query& filter(VertexPredicate predicate);            // arbitrary predicate filter
    Query& take(std::size_t n);
    Query& unique();
    Query& as(std::string tag);                          // mark current position
    Query& back(const std::string& tag);                 // rewind gremlin to a mark

    // Runs the pipeline to completion, returning every gremlin that made it
    // through (path history included, so you can inspect how it got there).
    std::vector<Gremlin> run();

    // Convenience: run() + extract just the resulting vertex ids.
    std::vector<VertexId> ids();

private:
    Graph& graph_;
    std::vector<std::unique_ptr<PipelineStep>> steps_;
    std::unordered_map<std::string, std::size_t> marks_; // tag -> hop index
    std::size_t hopCount_ = 0; // number of traversal steps (v/out/in/both) so far

    PipelineStep& lastStep(); // steps_ must be non-empty
};

} // namespace dagoba
