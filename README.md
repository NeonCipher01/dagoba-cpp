# dagoba-cpp

A small in-memory graph database I built in C++20, inspired by
[*Dagoba: An In-Memory Graph Database*](https://aosabook.org/en/500L/dagoba-an-in-memory-graph-database.html)
from the *500 Lines or Less* book. The original is written in JavaScript and
leans heavily on dynamic typing and closures, so this isn't a line-by-line
translation — I took the two core ideas from the article and rebuilt them
from scratch using real C++ types and a different execution model.

## What it actually does

You can add vertices and edges to a graph, then run chainable queries like:

```cpp
g.v({"alice"}).out("knows").take(2).ids();
```

which reads as "start at alice, walk out along 'knows' edges, take the first
2, give me their ids." Under the hood that's doing two things that make this
a *graph* database rather than just a wrapper around a couple of hash maps:

1. **Index-free adjacency.** Each `Vertex` stores its own list of outgoing
   and incoming edge ids (see `outEdgeIds`/`inEdgeIds` in `types.hpp`, and
   `Graph::outVertices`/`inVertices` in `graph.cpp`). Traversing from a vertex
   never scans the whole edge table — it only looks at that vertex's own
   edges. This is the one property that actually distinguishes a graph
   database from doing joins in a relational table.

2. **A lazy, pull-based query pipeline.** Calling `.out()`, `.filter()`,
   `.take()`, etc. doesn't compute anything — it just builds up a chain of
   step objects. Nothing actually runs until you call `.run()` or `.ids()`,
   and even then, each step only pulls from the step before it exactly as
   much as it needs. `tests/test_dagoba.cpp` has a test
   (`test_take_is_lazy`) that proves this directly: `take(1)` on a filter
   only evaluates the filter once, not across the whole graph.

## How the pipeline is built (and why it's not a copy of the original)

The original Dagoba runs its pipeline as one flat array of steps, walked by
a single loop that passes around one shared variable which is *either* real
data *or* one of two special strings, `'pull'` and `'done'`, used as control
signals. That works fine in JavaScript, where a variable can hold anything.
In C++ I didn't want a value that's sometimes real data and sometimes a
disguised instruction, so I modeled it differently: every step is its own
small class with one method, `next() -> std::optional<Gremlin>`
(`PipelineStep` in `query.hpp`). A step just calls `next()` on the step
before it whenever it needs more input, and returns `std::nullopt` when it's
out of things to produce. It's the same "don't compute until asked" idea,
just expressed as a chain of pull-based iterators instead of a hand-rolled
signal loop — closer to how you'd normally write this kind of thing in C++
(or Rust iterators, or C++20 ranges), rather than porting a JS-specific trick.

A couple of other things came out differently as a result:
- `Gremlin` (the little traveler that walks the graph mid-query) explicitly
  carries its own `path: vector<VertexId>`, and `as()`/`back()` resolve
  which path position a tag refers to at *build* time, inside `Query::as`,
  rather than figuring it out while the query runs.
- Vertex/edge properties are plain `string -> string` maps. That's a real
  simplification, not a hidden feature — see the ideas below for how I'd
  extend it.

## Building it

You need CMake 3.16+ and a C++20 compiler (I used g++ 13).

```bash
mkdir build && cd build
cmake ..
make
./dagoba_demo      # runs the demo in src/main.cpp
ctest              # runs the test suite
```

Or skip CMake entirely:

```bash
g++ -std=c++20 -Wall -Wextra -Iinclude src/graph.cpp src/query.cpp src/main.cpp -o demo
./demo
```

## What you can actually call

`v()`, `out()`, `in()`, `both()`, `has()`, `filter()`, `take()`, `unique()`,
`as()`, `back()` — that's the full set implemented in `query.cpp`. `has()`
isn't its own mechanism, by the way — it just builds a property-equality
check and calls `filter()` under the hood.

```cpp
Graph g;
g.addVertex("alice", {{"name", "Alice"}});
g.addVertex("bob",   {{"name", "Bob"}});
g.addEdge("alice", "bob", "knows");

auto ids = g.v({"alice"})
            .out("knows")
            .filter([](auto& v){ return v.properties.at("name") != "Bob"; })
            .take(5)
            .ids();
```

## Things I'd add next, if I kept going

These aren't implemented — just what I think would be worth building on top
of this, roughly in the order I'd tackle them:

- **A benchmark** comparing this index-free traversal against a naive
  "scan every edge" version on a large graph. This is really the whole
  point the original article makes, and I'd like actual numbers to back it up.
- **Typed properties** instead of `string -> string` — something like
  `std::variant<string, int64_t, double, bool>`.
- **Persistence** — save/load a `Graph` to disk.
- **A simple query optimizer** — e.g. pushing cheap `filter()` calls ahead
  of expensive traversals where that's safe to do.
- **Thread safety**, so multiple queries could run concurrently against a
  read-mostly graph.

## Layout

```
include/dagoba/   headers (types, graph, query)
src/              implementation + a runnable demo
tests/            a small hand-written test suite, no external framework needed
```
