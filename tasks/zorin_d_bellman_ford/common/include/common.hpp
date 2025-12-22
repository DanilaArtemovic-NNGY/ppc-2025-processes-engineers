#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "task/include/task.hpp"

namespace zorin_d_bellman_ford {

struct GraphCRS {
  int vertex_count{};
  std::vector<int> row_ptr;
  std::vector<int> col_idx;
  std::vector<int> weights;
};

struct InType {
  GraphCRS g;
  int source{};
};

using OutType = std::vector<std::int64_t>;
using TestType = std::tuple<int, std::string>;
using BaseTask = ppc::task::Task<InType, OutType>;

constexpr std::int64_t k_inf = std::numeric_limits<std::int64_t>::max() / 4;

inline GraphCRS MakeGraphCRS_Deterministic(int vertex_count, int edges_per_vertex) {
  GraphCRS gr;
  gr.vertex_count = vertex_count;
  gr.row_ptr.resize(static_cast<std::size_t>(vertex_count) + 1, 0);

  const int e_per_v = (edges_per_vertex <= 0) ? 1 : edges_per_vertex;

  gr.col_idx.reserve(static_cast<std::size_t>(vertex_count) * static_cast<std::size_t>(e_per_v));
  gr.weights.reserve(static_cast<std::size_t>(vertex_count) * static_cast<std::size_t>(e_per_v));

  int edge_pos = 0;
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    gr.row_ptr[static_cast<std::size_t>(vertex)] = edge_pos;
    for (int k = 1; k <= e_per_v; ++k) {
      const int to = (vertex + k) % vertex_count;
      const int w = 1 + ((vertex * 31 + to * 17 + k * 13) % 20);  // 1..20
      gr.col_idx.push_back(to);
      gr.weights.push_back(w);
      ++edge_pos;
    }
  }
  gr.row_ptr[static_cast<std::size_t>(vertex_count)] = edge_pos;
  return gr;
}

inline InType MakeInput(int vertex_count, int edges_per_vertex, int source) {
  InType in;
  in.g = MakeGraphCRS_Deterministic(vertex_count, edges_per_vertex);
  in.source = source;
  return in;
}

}  // namespace zorin_d_bellman_ford
