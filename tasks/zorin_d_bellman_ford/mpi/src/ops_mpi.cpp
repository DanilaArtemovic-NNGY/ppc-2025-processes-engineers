#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"

#include <mpi.h>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

ZorinDBellmanFordMPI::ZorinDBellmanFordMPI(const InType& in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput().clear();
}

bool ZorinDBellmanFordMPI::ValidationImpl() {
  const auto& in = GetInput();
  const auto& g = in.g;

  if (g.vertex_count <= 0) return false;
  if (in.source < 0 || in.source >= g.vertex_count) return false;

  if (g.row_ptr.size() != static_cast<std::size_t>(g.vertex_count) + 1) return false;
  if (g.row_ptr.empty() || g.row_ptr.front() != 0) return false;

  if (g.col_idx.size() != g.weights.size()) return false;
  const int e = static_cast<int>(g.col_idx.size());
  if (g.row_ptr.back() != e) return false;

  for (std::size_t i = 1; i < g.row_ptr.size(); ++i) {
    if (g.row_ptr[i] < g.row_ptr[i - 1]) return false;
  }
  for (int v : g.col_idx) {
    if (v < 0 || v >= g.vertex_count) return false;
  }

  return true;
}

bool ZorinDBellmanFordMPI::PreProcessingImpl() {
  const int v = GetInput().g.vertex_count;
  GetOutput().assign(static_cast<std::size_t>(v), kInf);
  GetOutput()[static_cast<std::size_t>(GetInput().source)] = 0;
  return true;
}

bool ZorinDBellmanFordMPI::RunImpl() {
  int rank = 0;
  int size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const auto& g = GetInput().g;
  const int V = g.vertex_count;

  std::vector<long long> dist = GetOutput();
  std::vector<long long> dist_next(static_cast<std::size_t>(V));

  for (int iter = 0; iter < V - 1; ++iter) {
    dist_next = dist;
    bool local_updated = false;

    for (int u = rank; u < V; u += size) {
      const long long du = dist[static_cast<std::size_t>(u)];
      if (du >= kInf / 2) continue;

      const int begin = g.row_ptr[static_cast<std::size_t>(u)];
      const int end   = g.row_ptr[static_cast<std::size_t>(u + 1)];

      for (int ei = begin; ei < end; ++ei) {
        const int v = g.col_idx[static_cast<std::size_t>(ei)];
        const long long cand = du + static_cast<long long>(g.weights[static_cast<std::size_t>(ei)]);
        auto& dv = dist_next[static_cast<std::size_t>(v)];
        if (cand < dv) {
          dv = cand;
          local_updated = true;
        }
      }
    }

    MPI_Allreduce(dist_next.data(), dist.data(), V, MPI_LONG_LONG, MPI_MIN, MPI_COMM_WORLD);

    int upd = local_updated ? 1 : 0;
    MPI_Allreduce(MPI_IN_PLACE, &upd, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    if (upd == 0) break;
  }

  GetOutput() = std::move(dist);
  return true;
}


bool ZorinDBellmanFordMPI::PostProcessingImpl() {
  return !GetOutput().empty();
}

}  // namespace zorin_d_bellman_ford
