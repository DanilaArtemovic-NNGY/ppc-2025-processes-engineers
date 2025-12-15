#pragma once

#include <string>
#include <tuple>

#include "task/include/task.hpp"

namespace zorin_d_ruler {

using InType = int;
using OutType = int;
using TestType = std::tuple<int, std::string>;
using BaseTask = ppc::task::Task<InType, OutType>;

}  // namespace zorin_d_ruler
