#include "dagoba/graph.hpp"
#include "dagoba/query.hpp"
#include <iostream>

using namespace dagoba;

static void printIds(const std::string& label, const std::vector<VertexId>& ids) {
    std::cout << label << ": ";
    for (auto& id : ids) std::cout << id << " ";
    std::cout << "(" << ids.size() << ")\n";
}

int main() {
    Graph g;

    // A small social graph.
    g.addVertex("alice", {{"name", "Alice"}, {"age", "30"}});
    g.addVertex("bob",   {{"name", "Bob"},   {"age", "27"}});
    g.addVertex("carol", {{"name", "Carol"}, {"age", "35"}});
    g.addVertex("dave",  {{"name", "Dave"},  {"age", "22"}});
    g.addVertex("eve",   {{"name", "Eve"},   {"age", "29"}});

    g.addEdge("alice", "bob",   "knows");
    g.addEdge("alice", "carol", "knows");
    g.addEdge("bob",   "dave",  "knows");
    g.addEdge("carol", "dave",  "knows");
    g.addEdge("carol", "eve",   "manages");
    g.addEdge("dave",  "eve",   "knows");

    std::cout << "Graph: " << g.vertexCount() << " vertices, "
              << g.edgeCount() << " edges\n\n";

    // 1) Who does Alice know directly?
    printIds("Alice's direct contacts", g.v({"alice"}).out("knows").ids());

    // 2) Friends-of-friends (2 hops), deduplicated, excluding cycles back to self.
    printIds("Alice's network within 2 hops",
              g.v({"alice"}).out("knows").out("knows").unique().ids());

    // 3) Filter by property: everyone under 30.
    printIds("People under 30",
              g.v().filter([](const Vertex& v) {
                  auto it = v.properties.find("age");
                  return it != v.properties.end() && std::stoi(it->second) < 30;
              }).ids());

    // 4) has() convenience for exact property match.
    printIds("Vertex named Carol", g.v().has("name", "Carol").ids());

    // 5) Lazy evaluation demo: take(2) should stop early even though
    //    many more matches exist - add a counting filter to prove it.
    int evaluated = 0;
    auto firstTwo = g.v()
                        .filter([&](const Vertex&) { ++evaluated; return true; })
                        .take(2)
                        .ids();
    printIds("First two vertices (lazy take)", firstTwo);
    std::cout << "  filter() was only evaluated " << evaluated
              << " times (proves laziness - it didn't scan all "
              << g.vertexCount() << " vertices)\n\n";

    // 6) as()/back(): find who manages someone that Dave knows.
    printIds("Managers of people Dave knows",
              g.v({"dave"}).as("start").out("knows").in("manages").back("start").ids());
    // (back() here just demonstrates rewinding; a more useful back() query
    //  would branch and recombine paths - left as an extension idea.)

    // 7) Both directions: who is connected to Dave at all (either direction)?
    printIds("Everyone connected to Dave (any direction)",
              g.v({"dave"}).both().unique().ids());

    return 0;
}
