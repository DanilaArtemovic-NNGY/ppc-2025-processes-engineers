#include "zorin_d_ruler/seq/include/ops_seq.hpp"

#include <cstdint>

#include "zorin_d_ruler/common/include/common.hpp"

namespace zorin_d_ruler {

namespace {

inline std::int64_t DoHeavyWork(int n) {
  std::int64_t acc = 0;
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k < n; ++k) {
        acc += (static_cast<std::int64_t>(i) * 31) + (static_cast<std::int64_t>(j) * 17) +
               (static_cast<std::int64_t>(k) * 13);
        acc ^= (acc << 1);
        acc += (acc >> 3);
      }
    }
  }
  return acc;
}

}  // namespace

ZorinDRulerSEQ::ZorinDRulerSEQ(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput() = 0;
}

bool ZorinDRulerSEQ::ValidationImpl() {
  return GetInput() > 0;
}

bool ZorinDRulerSEQ::PreProcessingImpl() {
  GetOutput() = 0;
  return true;
}

bool ZorinDRulerSEQ::RunImpl() {
  const int n = GetInput();
  if (n <= 0) {
    return false;
  }

  const std::int64_t w = DoHeavyWork(n);
  if (w == -1) {
    GetOutput() = -1;
    return false;
  }

  GetOutput() = n;
  return true;
}

bool ZorinDRulerSEQ::PostProcessingImpl() {
  GetOutput() = GetInput();
  return true;
}

}  // namespace zorin_d_ruler
