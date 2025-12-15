#include <gtest/gtest.h>

#include "util/include/perf_test_util.hpp"
#include "zorin_d_ruler/common/include/common.hpp"
#include "zorin_d_ruler/mpi/include/ops_mpi.hpp"
#include "zorin_d_ruler/seq/include/ops_seq.hpp"

namespace zorin_d_ruler {

class ZorinDRulerPerfTests : public ppc::util::BaseRunPerfTests<InType, OutType> {
  const int kCount_ = 550;
  InType input_data_{};

  void SetUp() override {
    input_data_ = kCount_;
  }

  bool CheckTestOutputData(OutType &output_data) final {
    return input_data_ == output_data;
  }

  InType GetTestInputData() final {
    return input_data_;
  }
};

TEST_P(ZorinDRulerPerfTests, RunPerfModes) {
  ExecuteTest(GetParam());
}

const auto kAllPerfTasks =
    ppc::util::MakeAllPerfTasks<InType, ZorinDRulerMPI, ZorinDRulerSEQ>(PPC_SETTINGS_example_processes_2);

const auto kGtestValues = ppc::util::TupleToGTestValues(kAllPerfTasks);

const auto kPerfTestName = ZorinDRulerPerfTests::CustomPerfTestName;

INSTANTIATE_TEST_SUITE_P(RunModeTests, ZorinDRulerPerfTests, kGtestValues, kPerfTestName);

}  // namespace zorin_d_ruler
