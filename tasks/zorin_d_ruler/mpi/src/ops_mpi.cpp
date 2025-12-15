#include "zorin_d_ruler/mpi/include/ops_mpi.hpp"

#include <mpi.h>

#include <algorithm>
#include <cstdint>

#include "zorin_d_ruler/common/include/common.hpp"

namespace zorin_d_ruler {

namespace {

inline std::int64_t DoHeavyWork(int n, int i_start, int i_end) {
  std::int64_t acc = 0;
  for (int i = i_start; i < i_end; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k < n; ++k) {
        acc += (static_cast<std::int64_t>(i) * 31) + (static_cast<std::int64_t>(j) * 17) +
               (static_cast<std::int64_t>(k) * 13);
        acc ^= (acc << 1);
        acc += (acc >> 3);
      }
    }
  }
  return acc;
}

inline std::int64_t LineAllSum(std::int64_t local, int rank, int size, MPI_Comm comm) {
  std::int64_t partial = local;

  if (rank > 0) {
    std::int64_t left = 0;
    MPI_Recv(&left, 1, MPI_INT64_T, rank - 1, 100, comm, MPI_STATUS_IGNORE);
    partial += left;
  }
  if (rank < size - 1) {
    MPI_Send(&partial, 1, MPI_INT64_T, rank + 1, 100, comm);
  }

  std::int64_t global = 0;
  if (rank == size - 1) {
    global = partial;
  }
  if (rank < size - 1) {
    MPI_Recv(&global, 1, MPI_INT64_T, rank + 1, 101, comm, MPI_STATUS_IGNORE);
  }
  if (rank > 0) {
    MPI_Send(&global, 1, MPI_INT64_T, rank - 1, 101, comm);
  }

  return global;
}

}  // namespace

ZorinDRulerMPI::ZorinDRulerMPI(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput() = 0;
}

bool ZorinDRulerMPI::ValidationImpl() {
  return GetInput() > 0;
}

bool ZorinDRulerMPI::PreProcessingImpl() {
  GetOutput() = 0;
  return true;
}

bool ZorinDRulerMPI::RunImpl() {
  int rank = 0;
  int size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const int n = GetInput();
  if (n <= 0) {
    return false;
  }

  const int base = n / size;
  const int rem = n % size;

  const int i_start = (rank * base) + std::min(rank, rem);
  const int i_end = i_start + base + (rank < rem ? 1 : 0);

  const std::int64_t local_work = DoHeavyWork(n, i_start, i_end);

  const std::int64_t global_work = LineAllSum(local_work, rank, size, MPI_COMM_WORLD);

  if (global_work == -1) {
    GetOutput() = -1;
    return false;
  }

  GetOutput() = n;
  return true;
}

bool ZorinDRulerMPI::PostProcessingImpl() {
  GetOutput() = GetInput();
  return true;
}

}  // namespace zorin_d_ruler
