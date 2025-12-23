#pragma once

#include <cstdint>
#include <vector>

#include "task/include/task.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordMPI : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kMPI;
  }

  explicit ZorinDBellmanFordMPI(const InType &in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;
  static bool RelaxIteration(int rank, int size, const GraphCrs &graph, const std::vector<std::int64_t> &dist,
                             std::vector<std::int64_t> &dist_next);
};

}  // namespace zorin_d_bellman_ford
