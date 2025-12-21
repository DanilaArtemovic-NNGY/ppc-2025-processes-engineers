#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "util/include/perf_test_util.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"
#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"
#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordPerfTests : public ppc::util::BaseRunPerfTests<InType, OutType> {
 protected:
  const int kV_ = 8000;
  const int kEdgesPerVertex_ = 8;

  InType input_data_{};

  void SetUp() override {
    input_data_ = MakeInput(kV_, kEdgesPerVertex_, 0);
  }

  bool CheckTestOutputData(OutType &output_data) final {
    return !output_data.empty() && output_data.size() == static_cast<std::size_t>(input_data_.g.vertex_count) &&
           output_data[0] == 0;
  }

  InType GetTestInputData() final {
    return input_data_;
  }
};

TEST_P(ZorinDBellmanFordPerfTests, RunPerfModes) {
  const auto &param = GetParam();

  const auto &name = std::get<1>(param);
  const auto run_type = std::get<2>(param);

#if defined(_WIN32)
  // На Windows pipeline для SEQ в PPC может давать неадекватные замеры/таймауты. Больше 300 секунд. Поэтому скип
  std::string_view name_sv{name};
  if (name_sv.find("_seq_") != std::string_view::npos &&
      run_type == ppc::performance::PerfResults::TypeOfRunning::kPipeline) {
    GTEST_SKIP() << "Skip SEQ pipeline on Windows";
  }
#endif

  ExecuteTest(param);
}

const auto kAllPerfTasks =
    ppc::util::MakeAllPerfTasks<InType, ZorinDBellmanFordMPI, ZorinDBellmanFordSEQ>(PPC_SETTINGS_zorin_d_bellman_ford);

const auto kGtestValues = ppc::util::TupleToGTestValues(kAllPerfTasks);
const auto kPerfTestName = ZorinDBellmanFordPerfTests::CustomPerfTestName;

INSTANTIATE_TEST_SUITE_P(RunModeTests, ZorinDBellmanFordPerfTests, kGtestValues, kPerfTestName);

}  // namespace zorin_d_bellman_ford
