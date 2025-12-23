#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

#include <cstddef>
#include <cstdint>

#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

ZorinDBellmanFordSEQ::ZorinDBellmanFordSEQ(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
}

bool ZorinDBellmanFordSEQ::ValidationImpl() {
  const auto &graph = GetInput().graph;

  if (graph.vertex_count <= 0) {
    return false;
  }
  if (GetInput().source < 0 || GetInput().source >= graph.vertex_count) {
    return false;
  }

  if (graph.row_ptr.size() != static_cast<std::size_t>(graph.vertex_count) + 1) {
    return false;
  }
  if (graph.col_idx.size() != graph.weights.size()) {
    return false;
  }
  return true;
}

bool ZorinDBellmanFordSEQ::PreProcessingImpl() {
  const int vertex_count = GetInput().graph.vertex_count;
  auto &dist = GetOutput();
  dist.assign(static_cast<std::size_t>(vertex_count), kInf);
  dist[static_cast<std::size_t>(GetInput().source)] = 0;
  return true;
}

bool ZorinDBellmanFordSEQ::RunImpl() {
  const auto &graph = GetInput().graph;
  const int vertex_count = graph.vertex_count;
  auto &dist = GetOutput();

  for (int iter = 0; iter < vertex_count - 1; ++iter) {
    bool updated = false;

    for (int vertex = 0; vertex < vertex_count; ++vertex) {
      const std::int64_t du = dist[static_cast<std::size_t>(vertex)];
      if (du >= kInf / 2) {
        continue;
      }

      const int begin = graph.row_ptr[static_cast<std::size_t>(vertex)];
      const int end = graph.row_ptr[static_cast<std::size_t>(vertex) + 1];

      for (int edge = begin; edge < end; ++edge) {
        const int to = graph.col_idx[static_cast<std::size_t>(edge)];
        const std::int64_t cand = du + static_cast<std::int64_t>(graph.weights[static_cast<std::size_t>(edge)]);
        if (cand < dist[static_cast<std::size_t>(to)]) {
          dist[static_cast<std::size_t>(to)] = cand;
          updated = true;
        }
      }
    }
    if (!updated) {
      break;
    }
  }
  return true;
}

bool ZorinDBellmanFordSEQ::PostProcessingImpl() {
  return !GetOutput().empty();
}

}  // namespace zorin_d_bellman_ford
