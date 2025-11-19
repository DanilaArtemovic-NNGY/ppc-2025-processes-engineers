#include "zorin_d_avg_vec/mpi/include/ops_mpi.hpp"

#include <mpi.h>

#include <vector>
#include <cstddef>
#include <algorithm>

#include "zorin_d_avg_vec/common/include/common.hpp"

namespace zorin_d_avg_vec {

ZorinDAvgVecMPI::ZorinDAvgVecMPI(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput() = 0.0;
}

bool ZorinDAvgVecMPI::ValidationImpl() {
  return true;
}

bool ZorinDAvgVecMPI::PreProcessingImpl() {
  return true;
}

bool ZorinDAvgVecMPI::RunImpl() {
  int rank = 0;
  int size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const auto &vec = GetInput();
  size_t total_size = vec.size();

  if (total_size == 0) {
    GetOutput() = 0.0;
    return true;
  }

  size_t chunk = total_size / size;
  size_t remainder = total_size % size;

  size_t start = (rank * chunk) + std::min(rank, static_cast<int>(remainder));
  size_t end = start + chunk + (std::cmp_less(rank, remainder) ? 1 : 0);

  double local_sum = 0;
  for (size_t i = start; i < end; ++i) {
    local_sum += vec[i];
  }

  double global_sum = 0;
  MPI_Allreduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  double avg = global_sum / static_cast<double>(total_size);

  GetOutput() = avg;

  return true;
}

bool ZorinDAvgVecMPI::PostProcessingImpl() {
  return true;
}

}  // namespace zorin_d_avg_vec
