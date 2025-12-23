#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"

#include <mpi.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

ZorinDBellmanFordMPI::ZorinDBellmanFordMPI(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
}

bool ZorinDBellmanFordMPI::ValidationImpl() {
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
  if (graph.row_ptr.front() != 0) {
    return false;
  }
  if (graph.col_idx.size() != graph.weights.size()) {
    return false;
  }
  if (graph.row_ptr.back() != static_cast<int>(std::ssize(graph.col_idx))) {
    return false;
  }

  if (!std::ranges::all_of(graph.col_idx, [&](int v) { return v >= 0 && v < graph.vertex_count; })) {
    return false;
  }
  return true;
}

bool ZorinDBellmanFordMPI::PreProcessingImpl() {
  const int vertex_count = GetInput().graph.vertex_count;
  auto &dist = GetOutput();
  dist.assign(static_cast<std::size_t>(vertex_count), kInf);
  dist[static_cast<std::size_t>(GetInput().source)] = 0;
  return true;
}

bool ZorinDBellmanFordMPI::RelaxIteration(int rank, int size, const GraphCrs &graph,
                                          const std::vector<std::int64_t> &dist, std::vector<std::int64_t> &dist_next) {
  bool updated = false;
  const int vertex_count = graph.vertex_count;

  for (int vertex = rank; vertex < vertex_count; vertex += size) {
    const std::int64_t du = dist[static_cast<std::size_t>(vertex)];
    if (du >= kInf / 2) {
      continue;
    }

    const int begin = graph.row_ptr[static_cast<std::size_t>(vertex)];
    const int end = graph.row_ptr[static_cast<std::size_t>(vertex) + 1];

    for (int edge = begin; edge < end; ++edge) {
      const int to = graph.col_idx[static_cast<std::size_t>(edge)];
      const std::int64_t cand = du + static_cast<std::int64_t>(graph.weights[static_cast<std::size_t>(edge)]);
      if (cand < dist_next[static_cast<std::size_t>(to)]) {
        dist_next[static_cast<std::size_t>(to)] = cand;
        updated = true;
      }
    }
  }
  return updated;
}

bool ZorinDBellmanFordMPI::RunImpl() {
  int rank = 0;
  int size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const auto &graph = GetInput().graph;
  const int vertex_count = graph.vertex_count;

  std::vector<std::int64_t> dist = GetOutput();
  std::vector<std::int64_t> dist_next(dist);

  for (int iter = 0; iter < vertex_count - 1; ++iter) {
    dist_next = dist;

    const bool local_updated = RelaxIteration(rank, size, graph, dist, dist_next);

    MPI_Allreduce(dist_next.data(), dist.data(), vertex_count, MPI_LONG, MPI_MIN, MPI_COMM_WORLD);

    int updated = local_updated ? 1 : 0;
    MPI_Allreduce(MPI_IN_PLACE, &updated, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    if (updated == 0) {
      break;
    }
  }

  GetOutput() = std::move(dist);
  return true;
}

bool ZorinDBellmanFordMPI::PostProcessingImpl() {
  return !GetOutput().empty();
}

}  // namespace zorin_d_bellman_ford
