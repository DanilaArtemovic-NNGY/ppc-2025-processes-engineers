#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
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

using OutType = std::vector<long long>;
using TestType = std::tuple<int, std::string>;
using BaseTask = ppc::task::Task<InType, OutType>;

constexpr long long kInf = std::numeric_limits<long long>::max() / 4;

inline GraphCRS MakeGraphCRS_Deterministic(int v, int edges_per_vertex) {
  GraphCRS gr;
  gr.vertex_count = v;
  gr.row_ptr.resize(static_cast<std::size_t>(v) + 1, 0);

  const int e_per_v = (edges_per_vertex <= 0) ? 1 : edges_per_vertex;

  gr.col_idx.reserve(static_cast<std::size_t>(v) * static_cast<std::size_t>(e_per_v));
  gr.weights.reserve(static_cast<std::size_t>(v) * static_cast<std::size_t>(e_per_v));

  int edge_pos = 0;
  for (int u = 0; u < v; ++u) {
    gr.row_ptr[static_cast<std::size_t>(u)] = edge_pos;
    for (int k = 1; k <= e_per_v; ++k) {
      const int to = (u + k) % v;
      const int w = 1 + ((u * 31 + to * 17 + k * 13) % 20);  // 1..20
      gr.col_idx.push_back(to);
      gr.weights.push_back(w);
      ++edge_pos;
    }
  }
  gr.row_ptr[static_cast<std::size_t>(v)] = edge_pos;
  return gr;
}

inline InType MakeInput(int v, int edges_per_vertex, int source) {
  InType in;
  in.g = MakeGraphCRS_Deterministic(v, edges_per_vertex);
  in.source = source;
  return in;
}

}  // namespace zorin_d_bellman_ford
