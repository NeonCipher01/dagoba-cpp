// Minimal hand-rolled test harness - no external dependencies required.
#include "dagoba/graph.hpp"
#include "dagoba/query.hpp"
#include <iostream>
#include <algorithm>
#include <cassert>

using namespace dagoba;

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << #cond << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        ++failures; \
    } \
} while (0)

static Graph buildSampleGraph() {
    Graph g;
    g.addVertex("a", {{"name", "A"}, {"age", "10"}});
    g.addVertex("b", {{"name", "B"}, {"age", "20"}});
    g.addVertex("c", {{"name", "C"}, {"age", "30"}});
    g.addEdge("a", "b", "knows");
    g.addEdge("b", "c", "knows");
    g.addEdge("a", "c", "manages");
    return g;
}

static bool contains(const std::vector<VertexId>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

void test_basic_traversal() {
    Graph g = buildSampleGraph();
    auto ids = g.v({"a"}).out("knows").ids();
    CHECK(ids.size() == 1);
    CHECK(contains(ids, "b"));
}

void test_two_hop_traversal() {
    Graph g = buildSampleGraph();
    auto ids = g.v({"a"}).out("knows").out("knows").ids();
    CHECK(ids.size() == 1);
    CHECK(contains(ids, "c"));
}

void test_in_traversal() {
    Graph g = buildSampleGraph();
    auto ids = g.v({"c"}).in("knows").ids();
    CHECK(ids.size() == 1);
    CHECK(contains(ids, "b"));
}

void test_both_traversal() {
    Graph g = buildSampleGraph();
    auto ids = g.v({"b"}).both().unique().ids();
    CHECK(ids.size() == 2);
    CHECK(contains(ids, "a"));
    CHECK(contains(ids, "c"));
}

void test_property_filter() {
    Graph g = buildSampleGraph();
    auto ids = g.v().has("name", "B").ids();
    CHECK(ids.size() == 1);
    CHECK(contains(ids, "b"));
}

void test_predicate_filter() {
    Graph g = buildSampleGraph();
    auto ids = g.v().filter([](const Vertex& v) {
        return std::stoi(v.properties.at("age")) >= 20;
    }).ids();
    CHECK(ids.size() == 2);
    CHECK(contains(ids, "b"));
    CHECK(contains(ids, "c"));
}

void test_take_is_lazy() {
    Graph g = buildSampleGraph();
    int evaluated = 0;
    auto ids = g.v().filter([&](const Vertex&) { ++evaluated; return true; }).take(1).ids();
    CHECK(ids.size() == 1);
    CHECK(evaluated == 1); // must not scan all 3 vertices
}

void test_unlabeled_edge_matches_any_label() {
    Graph g = buildSampleGraph();
    auto ids = g.v({"a"}).out().ids(); // no label filter: knows + manages
    CHECK(ids.size() == 2);
    CHECK(contains(ids, "b"));
    CHECK(contains(ids, "c"));
}

void test_label_filters_correctly() {
    Graph g = buildSampleGraph();
    auto ids = g.v({"a"}).out("manages").ids();
    CHECK(ids.size() == 1);
    CHECK(contains(ids, "c"));
}

void test_as_back_roundtrip() {
    Graph g = buildSampleGraph();
    // Walk a->b then rewind back to "a".
    auto ids = g.v({"a"}).as("start").out("knows").back("start").ids();
    CHECK(ids.size() == 1);
    CHECK(contains(ids, "a"));
}

void test_missing_vertex_addEdge_fails() {
    Graph g;
    g.addVertex("x");
    CHECK(g.addEdge("x", "nonexistent", "knows") == false);
}

void test_empty_v_returns_all_vertices() {
    Graph g = buildSampleGraph();
    auto ids = g.v().ids();
    CHECK(ids.size() == 3);
}

int main() {
    test_basic_traversal();
    test_two_hop_traversal();
    test_in_traversal();
    test_both_traversal();
    test_property_filter();
    test_predicate_filter();
    test_take_is_lazy();
    test_unlabeled_edge_matches_any_label();
    test_label_filters_correctly();
    test_as_back_roundtrip();
    test_missing_vertex_addEdge_fails();
    test_empty_v_returns_all_vertices();

    if (failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
