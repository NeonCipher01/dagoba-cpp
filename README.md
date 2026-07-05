# dagoba-cpp

A small in-memory graph database in modern C++20, inspired by the ideas in
[*Dagoba: An In-Memory Graph Database*](https://aosabook.org/en/500L/dagoba-an-in-memory-graph-database.html)
(500 Lines or Less). This is **not** a line-by-line port — the original is
JavaScript and leans heavily on dynamic typing and closures. This version
re-derives the same ideas (index-free adjacency, a lazy pull-based query
pipeline) using real C++ types.

## Core ideas ported over

1. **Index-free adjacency** — each `Vertex` stores its own outgoing/incoming
   edge ids directly (`Graph::outVertices`/`inVertices`), instead of scanning
   a global edge table for every traversal. This is *the* defining trait of a
   graph database vs. a relational one.
2. **A chainable, lazy query pipeline** — `g.v({"alice"}).out("knows").take(2)`
   builds up a pipeline that does no work until you call `.run()`/`.ids()`,
   and even then only pulls as much upstream data as it actually needs
   (see `test_take_is_lazy` — `take(1)` only evaluates one vertex, not the
   whole graph).

## Where this diverges from the original, and why

The original implements the pull-based evaluation as a flat array of steps
walked by a single loop using two string signals (`'pull'` / `'done'`) that
share a variable with the actual data — workable in JS, fragile in a
statically typed language. Here, each step is instead a small object
implementing `next() -> optional<Gremlin>` (`PipelineStep` in `query.hpp`),
and steps pull from their upstream step's `next()` directly. It's the same
lazy pull-based idea, just expressed as a chain of iterator-like objects
instead of a hand-rolled signal loop — arguably closer to how you'd model
this in idiomatic C++ (or Rust iterators, or C++20 ranges).

Other differences:
- `StepArgs` is a plain struct with typed fields, replacing JS's `*args` splat.
- `Gremlin` carries an explicit `path: vector<VertexId>` for `as()`/`back()`,
  and mark-to-hop-index resolution happens at **build time** in `Query::as`,
  not at runtime.
- Properties are `string -> string` for simplicity. See "Extension ideas" below.

## Build

Requires CMake 3.16+ and a C++20 compiler (tested with g++ 13).

```bash
mkdir build && cd build
cmake ..
make
./dagoba_demo      # run the demo
ctest              # run the test suite
```

Or without CMake:

```bash
g++ -std=c++20 -Wall -Wextra -Iinclude src/graph.cpp src/query.cpp src/main.cpp -o demo
./demo
```

## API sketch

```cpp
Graph g;
g.addVertex("alice", {{"name", "Alice"}});
g.addVertex("bob",   {{"name", "Bob"}});
g.addEdge("alice", "bob", "knows");

auto ids = g.v({"alice"})      // start at "alice"
            .out("knows")      // walk outgoing "knows" edges
            .filter([](auto& v){ return v.properties.at("name") != "Bob"; })
            .take(5)
            .ids();
```

Available steps: `v()`, `out()`, `in()`, `both()`, `has()`, `filter()`,
`take()`, `unique()`, `as()`, `back()`.

## Extension ideas (good for showing depth beyond the base project)

- **Persistence**: serialize `Graph` to/from disk (a simple line-based format,
  or JSON with a header-only library).
- **Typed properties**: replace `string -> string` properties with a
  `std::variant<string, int64_t, double, bool>` property value.
- **Benchmark**: compare this index-free adjacency traversal against a naive
  "scan all edges" implementation on a large random graph — quantify the
  speedup, which is the whole point the original article makes.
- **Query optimizer pass**: e.g. reorder `filter()` before expensive
  traversals where safe, or merge consecutive `unique()` calls.
- **Thread safety**: add a reader-writer lock so multiple queries can run
  concurrently against a read-mostly graph.
- **A tiny REPL**: parse a text query language into the `Query` builder calls.

## Layout

```
include/dagoba/   public headers (types, graph, query)
src/              implementation + demo main
tests/            self-contained test suite (no external framework needed)
```
