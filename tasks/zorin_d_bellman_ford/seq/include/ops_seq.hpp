#pragma once

#include "task/include/task.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordSEQ : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kSEQ;
  }

  explicit ZorinDBellmanFordSEQ(const InType &in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;
};

}  // namespace zorin_d_bellman_ford
