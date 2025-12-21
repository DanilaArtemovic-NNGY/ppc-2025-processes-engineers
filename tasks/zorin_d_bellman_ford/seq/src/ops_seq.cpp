#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

ZorinDBellmanFordSEQ::ZorinDBellmanFordSEQ(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput().clear();
}

bool ZorinDBellmanFordSEQ::ValidationImpl() {
  const auto &in = GetInput();
  const auto &g = in.g;

  if (g.vertex_count <= 0) {
    return false;
  }
  if (in.source < 0 || in.source >= g.vertex_count) {
    return false;
  }

  if (g.row_ptr.size() != static_cast<std::size_t>(g.vertex_count) + 1) {
    return false;
  }
  if (g.row_ptr.empty() || g.row_ptr.front() != 0) {
    return false;
  }

  if (g.col_idx.size() != g.weights.size()) {
    return false;
  }
  const int edges = static_cast<int>(g.col_idx.size());
  if (g.row_ptr.back() != edges) {
    return false;
  }

  for (std::size_t i = 1; i < g.row_ptr.size(); ++i) {
    if (g.row_ptr[i] < g.row_ptr[i - 1]) {
      return false;
    }
  }

  for (int v : g.col_idx) {
    if (v < 0 || v >= g.vertex_count) {
      return false;
    }
  }

  return true;
}

bool ZorinDBellmanFordSEQ::PreProcessingImpl() {
  const int v = GetInput().g.vertex_count;
  auto &dist = GetOutput();

  dist.assign(static_cast<std::size_t>(v), kInf);
  dist[static_cast<std::size_t>(GetInput().source)] = 0;

  return true;
}

bool ZorinDBellmanFordSEQ::RunImpl() {
  const auto &g = GetInput().g;
  const int V = g.vertex_count;
  auto &dist = GetOutput();

  for (int iter = 0; iter < V - 1; ++iter) {
    bool any_update = false;

    for (int u = 0; u < V; ++u) {
      const long long du = dist[static_cast<std::size_t>(u)];
      if (du >= kInf / 2) {
        continue;
      }

      const int begin = g.row_ptr[static_cast<std::size_t>(u)];
      const int end = g.row_ptr[static_cast<std::size_t>(u + 1)];

      for (int ei = begin; ei < end; ++ei) {
        const int v = g.col_idx[static_cast<std::size_t>(ei)];
        const long long cand = du + static_cast<long long>(g.weights[static_cast<std::size_t>(ei)]);
        auto &dv = dist[static_cast<std::size_t>(v)];
        if (cand < dv) {
          dv = cand;
          any_update = true;
        }
      }
    }

    if (!any_update) {
      break;
    }
  }

  return true;
}

bool ZorinDBellmanFordSEQ::PostProcessingImpl() {
  return !GetOutput().empty();
}

}  // namespace zorin_d_bellman_ford
