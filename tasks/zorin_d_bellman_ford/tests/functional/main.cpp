#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <tuple>

#include "util/include/func_test_util.hpp"
#include "util/include/util.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"
#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"
#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordFuncTests : public ppc::util::BaseRunFuncTests<InType, OutType, TestType> {
 public:
  static std::string PrintTestParam(const TestType &test_param) {
    return std::to_string(std::get<0>(test_param)) + "_" + std::get<1>(test_param);
  }

 protected:
  void SetUp() override {
    const auto &test_param = std::get<static_cast<std::size_t>(ppc::util::GTestParamIndex::kTestParams)>(GetParam());
    const int v = std::get<0>(test_param);

    input_data_ = MakeInput(v, 3, 0);
  }

  bool CheckTestOutputData(OutType &output_data) final {
    return !output_data.empty() && output_data.size() == static_cast<std::size_t>(input_data_.graph.vertex_count) &&
           output_data[0] == 0;
  }

  InType GetTestInputData() final {
    return input_data_;
  }

 private:
  InType input_data_{};
};

namespace {

TEST_P(ZorinDBellmanFordFuncTests, SeqMpiSameResult) {
  ExecuteTest(GetParam());
}

const std::array<TestType, 3> kTestParam = {
    std::make_tuple(10, "v10"),
    std::make_tuple(50, "v50"),
    std::make_tuple(100, "v100"),
};

const auto kTestTasksList =
    std::tuple_cat(ppc::util::AddFuncTask<ZorinDBellmanFordMPI, InType>(kTestParam, PPC_SETTINGS_zorin_d_bellman_ford),
                   ppc::util::AddFuncTask<ZorinDBellmanFordSEQ, InType>(kTestParam, PPC_SETTINGS_zorin_d_bellman_ford));

const auto kGtestValues = ppc::util::ExpandToValues(kTestTasksList);

const auto kFuncTestName = ZorinDBellmanFordFuncTests::PrintFuncTestName<ZorinDBellmanFordFuncTests>;

INSTANTIATE_TEST_SUITE_P(BellmanFordTests, ZorinDBellmanFordFuncTests, kGtestValues, kFuncTestName);

}  // namespace

}  // namespace zorin_d_bellman_ford
