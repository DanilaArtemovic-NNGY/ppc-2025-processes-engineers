#pragma once

#include "task/include/task.hpp"
#include "zorin_d_ruler/common/include/common.hpp"

namespace zorin_d_ruler {

class ZorinDRulerMPI : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kMPI;
  }
  explicit ZorinDRulerMPI(const InType &in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;
};

}  // namespace zorin_d_ruler
