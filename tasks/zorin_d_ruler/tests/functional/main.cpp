#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <tuple>

#include "util/include/func_test_util.hpp"
#include "util/include/util.hpp"
#include "zorin_d_ruler/common/include/common.hpp"
#include "zorin_d_ruler/mpi/include/ops_mpi.hpp"
#include "zorin_d_ruler/seq/include/ops_seq.hpp"

namespace zorin_d_ruler {

class ZorinDRulerFuncTests : public ppc::util::BaseRunFuncTests<InType, OutType, TestType> {
 public:
  static std::string PrintTestParam(const TestType &test_param) {
    return std::to_string(std::get<0>(test_param)) + "_" + std::get<1>(test_param);
  }

 protected:
  void SetUp() override {
    const auto &test_param = std::get<static_cast<std::size_t>(ppc::util::GTestParamIndex::kTestParams)>(GetParam());

    input_data_ = std::get<0>(test_param);
  }

  bool CheckTestOutputData(OutType &output_data) final {
    return output_data == input_data_;
  }

  InType GetTestInputData() final {
    return input_data_;
  }

 private:
  InType input_data_{0};
};

namespace {

TEST_P(ZorinDRulerFuncTests, LineTopology) {
  ExecuteTest(GetParam());
}

const std::array<TestType, 3> kTestParam = {
    std::make_tuple(10, "10"),
    std::make_tuple(50, "50"),
    std::make_tuple(100, "100"),
};

const auto kTestTasksList =
    std::tuple_cat(ppc::util::AddFuncTask<ZorinDRulerMPI, InType>(kTestParam, PPC_SETTINGS_example_processes_2),
                   ppc::util::AddFuncTask<ZorinDRulerSEQ, InType>(kTestParam, PPC_SETTINGS_example_processes_2));

const auto kGtestValues = ppc::util::ExpandToValues(kTestTasksList);

const auto kFuncTestName = ZorinDRulerFuncTests::PrintFuncTestName<ZorinDRulerFuncTests>;

INSTANTIATE_TEST_SUITE_P(LineTopologyTests, ZorinDRulerFuncTests, kGtestValues, kFuncTestName);

}  // namespace

}  // namespace zorin_d_ruler
