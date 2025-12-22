#include <gtest/gtest.h>

#include <cstddef>

#include "util/include/perf_test_util.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"
#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"
#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

namespace zorin_d_bellman_ford {

struct InTypeWrapper {
  InType value;
};

class ZorinDBellmanFordPerfTests : public ppc::util::BaseRunPerfTests<InTypeWrapper, OutType> {
 protected:
  const int vertex_count = 20000;
  const int edges_per_vertex = 64;

  InTypeWrapper input_data_{};

  void SetUp() override {
    input_data_.value = MakeInput(vertex_count, edges_per_vertex, 0);
  }

  bool CheckTestOutputData(OutType &output_data) final {
    return !output_data.empty() &&
           output_data.size() == static_cast<std::size_t>(input_data_.value.graph.vertex_count) && output_data[0] == 0;
  }

  InTypeWrapper GetTestInputData() final {
    return input_data_;
  }
};

TEST_P(ZorinDBellmanFordPerfTests, RunPerfModes) {
  ExecuteTest(GetParam());
}

const auto kAllPerfTasks = ppc::util::MakeAllPerfTasks<InTypeWrapper, ZorinDBellmanFordMPI, ZorinDBellmanFordSEQ>(
    PPC_SETTINGS_zorin_d_bellman_ford);

const auto kGtestValues = ppc::util::TupleToGTestValues(kAllPerfTasks);

const auto kPerfTestName = ZorinDBellmanFordPerfTests::CustomPerfTestName;

INSTANTIATE_TEST_SUITE_P(RunModeTests, ZorinDBellmanFordPerfTests, kGtestValues, kPerfTestName);

}  // namespace zorin_d_bellman_ford
