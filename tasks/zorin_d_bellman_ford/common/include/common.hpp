#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "task/include/task.hpp"

namespace zorin_d_bellman_ford {

struct GraphCrs {
  int vertex_count{};
  std::vector<int> row_ptr;
  std::vector<int> col_idx;
  std::vector<int> weights;
};

struct InType {
  GraphCrs graph;
  int source{};
};

using OutType = std::vector<std::int64_t>;
using TestType = std::tuple<int, std::string>;
using BaseTask = ppc::task::Task<InType, OutType>;

constexpr std::int64_t kInf = std::numeric_limits<std::int64_t>::max() / 4;

inline GraphCrs MakeGraphCrsDeterministic(int vertex_count, int edges_per_vertex) {
  GraphCrs graph;
  graph.vertex_count = vertex_count;
  graph.row_ptr.resize(static_cast<std::size_t>(vertex_count) + 1, 0);

  const int edges = edges_per_vertex > 0 ? edges_per_vertex : 1;
  const std::size_t total_edges = static_cast<std::size_t>(vertex_count) * static_cast<std::size_t>(edges);

  graph.col_idx.reserve(total_edges);
  graph.weights.reserve(total_edges);

  int edge_pos = 0;
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    graph.row_ptr[static_cast<std::size_t>(vertex)] = edge_pos;
    for (int k = 1; k <= edges; ++k) {
      const int to = (vertex + k) % vertex_count;
      const int weight = 1 + ((vertex * 31 + to * 17 + k * 13) % 20);
      graph.col_idx.push_back(to);
      graph.weights.push_back(weight);
      ++edge_pos;
    }
  }
  graph.row_ptr[static_cast<std::size_t>(vertex_count)] = edge_pos;
  return graph;
}

inline InType MakeInput(int vertex_count, int edges_per_vertex, int source) {
  return InType{.graph = MakeGraphCrsDeterministic(vertex_count, edges_per_vertex), .source = source};
}

}  // namespace zorin_d_bellman_ford
