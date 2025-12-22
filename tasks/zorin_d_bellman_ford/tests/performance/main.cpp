#include <gtest/gtest.h>

#include <cstddef>

#include "util/include/perf_test_util.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"
#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"
#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

// NOLINTBEGIN(performance-unnecessary-value-param)
// clang-tidy сообщает ложное срабатывание:
// InType передаётся по значению в коде modules/task

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordPerfTests : public ppc::util::BaseRunPerfTests<InType, OutType> {
 protected:
  const int vertex_count = 20000;
  const int edges_per_vertex = 64;

  InType input_data{};

  void SetUp() override {
    input_data = MakeInput(vertex_count, edges_per_vertex, 0);
  }

  bool CheckTestOutputData(OutType &output_data) final {
    return !output_data.empty() && output_data.size() == static_cast<std::size_t>(input_data.graph.vertex_count) &&
           output_data[0] == 0;
  }

  InType GetTestInputData() final {
    return input_data;
  }
};

TEST_P(ZorinDBellmanFordPerfTests, RunPerfModes) {
  const auto &param = GetParam();
  ExecuteTest(param);
}

const auto kAllPerfTasks =
    ppc::util::MakeAllPerfTasks<InType, ZorinDBellmanFordMPI, ZorinDBellmanFordSEQ>(PPC_SETTINGS_zorin_d_bellman_ford);

const auto kGtestValues = ppc::util::TupleToGTestValues(kAllPerfTasks);
const auto kPerfTestName = ZorinDBellmanFordPerfTests::CustomPerfTestName;

INSTANTIATE_TEST_SUITE_P(RunModeTests, ZorinDBellmanFordPerfTests, kGtestValues, kPerfTestName);

}  // namespace zorin_d_bellman_ford
// NOLINTEND(performance-unnecessary-value-param)
