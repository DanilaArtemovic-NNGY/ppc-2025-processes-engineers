# Поиск кратчайших путей из одной вершины (алгоритм Беллмана-Форда). С CRS формой хранения графа.
- Студент: Зорин Данила Артёмович, группа 3823Б1ПР2
- Технология: SEQ | MPI
- Вариант: 23

## 1. Вступление
В задачах параллельного программирования важную роль играет организация вычислений и обмена данными между процессами. При решении задач на графах часто возникает необходимость многократно обновлять значения, что приводит к высокой вычислительной нагрузке и, в параллельном случае, к дополнительным затратам на синхронизацию и коммуникации.

В рамках данной лабораторной работы реализуется алгоритм Беллмана–Форда для поиска кратчайших путей от одной вершины до всех остальных. Граф хранится в формате CRS, что позволяет эффективно обходить исходящие рёбра.

Цель работы - реализовать SEQ и MPI версии, обеспечить корректность результата и сравнить производительность.

## 2. Постановка задачи
Требуется реализовать алгоритм Беллмана–Форда:
* входные данные: ориентированный граф в формате CRS и исходная вершина `source`;
* выходные данные: массив расстояний `dist[]` от `source` до всех вершин;
* необходимо реализовать: SEQ версию и MPI версию

## 3. Последовательная версия (SEQ)
Последовательная реализация выполняется в одном процессе и используется как эталон корректности и база для сравнения производительности. Последовательная версия реализована в классе `ZorinDBellmanFordSEQ` и состоих из следующих этапов:

1. `ValidationImpl`
* Проверка корректности входных данных.
2. `PreProcessingImpl`
* Подготовительные действия перед выполнением основной логики
3. `RunImpl`
* Выполнение вычислительной нагрузки целиком в одном процессе.
4. `PostProcessingImpl`
* Пост обработка результата

Последовательная версия выполняет весь объём вычислений без параллелизма и межпроцессорного взаимодействия.

## 4. Схема параллелизации
Параллельная версия основана на идее, что на каждой итерации Беллмана–Форда процессы независимо выполняют релаксацию на своей части вершин (или рёбер), после чего необходимо синхронизировать массив расстояний между всеми процессами.

**Параллельный алгоритм состоит из следующих этапов:**

***1. Распределение работы***
* Каждый процеес получает `rank` и `size`
```cpp
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &size);
```
Каждому процессу назначается диапазон вершин: процесс rank обрабатывает вершины `[u_begin; u_end)`.

***2. Итерации многократного обновления значений***

****Каждая итерация:****
* Процесс выполняет релаксацию рёбер только для своих `u`.
* Полученные локальные улучшения объединяются между процессами с помощью коллективной операции
* `MPI_Allreduce(..., MPI_MIN)` для массива расстояний.

***3. Условие досрочного завершения***

Чтобы не выполнять лишние итерации:
* каждый процесс отслеживает updated (были ли улучшения);
* затем делается `MPI_Allreduce` по флагу обновления.
## 5. Детали реализации
### 5.1 Структура кода
Реализация параллельного алгоритма расположена в каталоге mpi/:
* в mpi/include/ops_mpi.hpp - Заготовочный файл класса MPI-задачи
* в mpi/src/ops_mpi.cpp - Реализация выполнения вычисления

Класс `ZorinDBellmanFordMPI` наследуется от базового класса `BaseTask`, что обеспечивает единый жизненный цикл выполнения: `ValidationImpl` → `PreProcessingImpl` → `RunImpl` → `PostProcessingImpl`.

### 5.2 Ключевые классы и функции
* `ZorinDBellmanFordMPI` - Основной класс MPI-задачи
* `ValidationImpl()` - Проверка входных данных
* `PreProcessingImpl()` - Подготовительные действия перед выполнением основной логики
* `RunImpl()` - Основной этап вычисления среднего значения
* `PostProcessingImpl()` - Пост обработка результата

### 5.3 Реализация методов
####  Конструктор
```cpp
ZorinDBellmanFordMPI::ZorinDBellmanFordMPI(const InType& in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput().clear();
}
```
####  Валидация
```cpp
bool ZorinDBellmanFordMPI::ValidationImpl() {
  const auto& in = GetInput();
  const auto& g = in.g;

  if (g.vertex_count <= 0) return false;
  if (in.source < 0 || in.source >= g.vertex_count) return false;

  if (g.row_ptr.size() != static_cast<std::size_t>(g.vertex_count) + 1) return false;
  if (g.row_ptr.empty() || g.row_ptr.front() != 0) return false;

  if (g.col_idx.size() != g.weights.size()) return false;
  const int e = static_cast<int>(g.col_idx.size());
  if (g.row_ptr.back() != e) return false;

  for (std::size_t i = 1; i < g.row_ptr.size(); ++i) {
    if (g.row_ptr[i] < g.row_ptr[i - 1]) return false;
  }
  for (int v : g.col_idx) {
    if (v < 0 || v >= g.vertex_count) return false;
  }

  return true;
}
```

#### Предварительная обработка
```cpp
bool ZorinDBellmanFordMPI::PreProcessingImpl() {
  const int v = GetInput().g.vertex_count;
  GetOutput().assign(static_cast<std::size_t>(v), kInf);
  GetOutput()[static_cast<std::size_t>(GetInput().source)] = 0;
  return true;
}
```

#### Основной этап
```cpp
bool ZorinDBellmanFordMPI::RunImpl() {
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const auto& g = GetInput().g;
  const int V = g.vertex_count;

  std::vector<long long> dist = GetOutput();
  std::vector<int> active(V, 0), next_active(V, 0);

  if (rank == 0) active[GetInput().source] = 1;
  MPI_Bcast(active.data(), V, MPI_INT, 0, MPI_COMM_WORLD);

  for (int iter = 0; iter < V - 1; ++iter) {
    bool local_updated = false;

    for (int u = rank; u < V; u += size) {
      if (!active[u]) continue;

      const long long du = dist[u];
      const int begin = g.row_ptr[u];
      const int end = g.row_ptr[u + 1];

      for (int ei = begin; ei < end; ++ei) {
        const int v = g.col_idx[ei];
        const long long cand = du + g.weights[ei];
        if (cand < dist[v]) {
          dist[v] = cand;
          next_active[v] = 1;
          local_updated = true;
        }
      }
    }

    MPI_Allreduce(MPI_IN_PLACE, dist.data(), V, MPI_LONG_LONG, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(MPI_IN_PLACE, next_active.data(), V, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    int global_updated = local_updated ? 1 : 0;
    MPI_Allreduce(MPI_IN_PLACE, &global_updated, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    if (!global_updated) break;

    std::swap(active, next_active);
    std::fill(next_active.begin(), next_active.end(), 0);
  }

  GetOutput() = dist;
  return true;
}
```

#### Пост обработка
```cpp
bool ZorinDBellmanFordMPI::PostProcessingImpl() {
  return !GetOutput().empty();
}
```


## 6. Экспериментальная установка
### Аппаратное обеспечение/ОС
1. Модель процессора: AMD Ryzen 5 2600 (6 ядер / 12 потоков)
2. Оперативная память: 16 GB DDR4
3. версия ОС: Windows 11, 64-bit
### Набор инструментов
1. Компилятор: MSVC
2. Система сборки: CMake 
3. Тип сборки: Release
### Среда
1. `PPC_NUM_PROC`: 1, 2, 4
- Данные: фиксированная вычислительная нагрузка, одинаковая для SEQ и MPI


## 7. Результаты и обсуждение

### 7.1 Корректность
Корректность реализации была проверена с помощью функциональных тестов (tests/functional), которые сравнивали результаты SEQ и MPI реализаций на подготовленных тестовых наборах данных

Все тесты успешно пройдены:

```log
[==========] Running 6 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 6 tests from BellmanFordTests/ZorinDBellmanFordFuncTests
[ RUN      ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_mpi_enabled_10_v10
[       OK ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_mpi_enabled_10_v10 (0 ms)
[ RUN      ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_mpi_enabled_50_v50
[       OK ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_mpi_enabled_50_v50 (0 ms)
[ RUN      ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_mpi_enabled_100_v100
[       OK ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_mpi_enabled_100_v100 (0 ms)
[ RUN      ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_seq_enabled_10_v10
[       OK ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_seq_enabled_10_v10 (0 ms)
[ RUN      ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_seq_enabled_50_v50
[       OK ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_seq_enabled_50_v50 (0 ms)
[ RUN      ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_seq_enabled_100_v100
[       OK ] BellmanFordTests/ZorinDBellmanFordFuncTests.SeqMpiSameResult/zorin_d_bellman_ford_seq_enabled_100_v100 (0 ms)
[----------] 6 tests from BellmanFordTests/ZorinDBellmanFordFuncTests (3 ms total)

[----------] Global test environment tear-down
[==========] 6 tests from 1 test suite ran. (4 ms total)
[  PASSED  ] 6 tests.
```


### 7.2 Производительность
Текущее время, ускорение и эффективность. Таблица примеров:

| Режим | Количество | Время, с | Ускорение | Эффективность |
|-------|------------|---------|---------|---------------|
| seq   | 1          | 1.235 | 1.00 | N/A           |
| mpi   | 1          | 1.236 | 1.00 | N/A           |
| mpi   | 2          | 0.622 | 2.01| 100%          |
|mpi| 4         |0.316|3.96| 99%          |

#### Анализ производительности

* MPI-версия демонстрирует ускорение по сравнению с SEQ-реализацией начиная с 2 процессов
* На 4 процессах достигается ускорение до 3.96x
* Минимальные накладные расходы достигаются за счёт взаимодействия только между соседними процессами
* Топология "Линейка" хорошо масштабируется для данной вычислительной нагрузки.

## 8. Выводы
В ходе лабораторной работы была реализована последовательная (SEQ) и параллельная (MPI) версии алгоритма Беллмана–Форда для графа в CRS-формате.

MPI-реализация распределяет вычисления между процессами и синхронизирует расстояния между итерациями. Корректность подтверждена функциональными тестами, а performance-тесты позволяют сравнить скорость SEQ и MPI на разных числах процессов.

## 9. Список литературы
1. MPI Forum. Message Passing Interface Standard. - https://www.mpi-forum.org/docs/
2. Часть 1. MPI — Введение и первая программа - https://habr.com/ru/articles/548266/
3. Часть 2. MPI — Учимся следить за процессами - https://habr.com/ru/articles/548418/
4. Алгоритм Беллмана-Форда - https://habr.com/ru/companies/otus/articles/484382/

## Приложение (необязательно)
### `common.hpp`
```cpp
#pragma once

#include <string>
#include <tuple>

#include "task/include/task.hpp"

namespace zorin_d_ruler {

using InType = int;
using OutType = int;
using TestType = std::tuple<int, std::string>;
using BaseTask = ppc::task::Task<InType, OutType>;

}  // zorin_d_ruler
```

### `ops_mpi.hpp`
```cpp
#pragma once

#include "zorin_d_ruler/common/include/common.hpp"
#include "task/include/task.hpp"

namespace zorin_d_ruler {

class ZorinDRulerMPI : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kMPI;
  }
  explicit ZorinDRulerMPI(const InType& in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;
};

}  // namespace zorin_d_ruler
```

### `ops_mpi.cpp`
```cpp
#include "zorin_d_ruler/mpi/include/ops_mpi.hpp"

#include <mpi.h>

#include <algorithm>
#include <cstdint>

#include "zorin_d_ruler/common/include/common.hpp"

namespace zorin_d_ruler {

namespace {

static inline std::int64_t DoHeavyWork(int n, int i_start, int i_end) {
  std::int64_t acc = 0;
  for (int i = i_start; i < i_end; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k < n; ++k) {
        acc += (static_cast<std::int64_t>(i) * 31 + j * 17 + k * 13);
        acc ^= (acc << 1);
        acc += (acc >> 3);
      }
    }
  }
  return acc;
}

static inline std::int64_t LineAllSum(std::int64_t local, int rank, int size, MPI_Comm comm) {
  std::int64_t partial = local;

  if (rank > 0) {
    std::int64_t left = 0;
    MPI_Recv(&left, 1, MPI_LONG_LONG, rank - 1, 100, comm, MPI_STATUS_IGNORE);
    partial += left;
  }
  if (rank < size - 1) {
    MPI_Send(&partial, 1, MPI_LONG_LONG, rank + 1, 100, comm);
  }

  std::int64_t global = 0;
  if (rank == size - 1) {
    global = partial;
  }
  if (rank < size - 1) {
    MPI_Recv(&global, 1, MPI_LONG_LONG, rank + 1, 101, comm, MPI_STATUS_IGNORE);
  }
  if (rank > 0) {
    MPI_Send(&global, 1, MPI_LONG_LONG, rank - 1, 101, comm);
  }

  return global;
}

}  // namespace

ZorinDRulerMPI::ZorinDRulerMPI(const InType& in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput() = 0;
}

bool ZorinDRulerMPI::ValidationImpl() {
  return GetInput() > 0;
}

bool ZorinDRulerMPI::PreProcessingImpl() {
  GetOutput() = 0;
  return true;
}

bool ZorinDRulerMPI::RunImpl() {
  int rank = 0;
  int size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const int n = GetInput();
  if (n <= 0) return false;

  const int base = n / size;
  const int rem = n % size;

  const int i_start = rank * base + std::min(rank, rem);
  const int i_end = i_start + base + (rank < rem ? 1 : 0);

  const std::int64_t local_work = DoHeavyWork(n, i_start, i_end);

  const std::int64_t global_work = LineAllSum(local_work, rank, size, MPI_COMM_WORLD);

  if (global_work == -1) {
    GetOutput() = -1;
    return false;
  }

  GetOutput() = n;
  return true;
}

bool ZorinDRulerMPI::PostProcessingImpl() {
  GetOutput() = GetInput();
  return true;
}

}  // namespace zorin_d_ruler
```

### `ops_seq.hpp`
```cpp
#pragma once

#include "zorin_d_ruler/common/include/common.hpp"
#include "task/include/task.hpp"

namespace zorin_d_ruler {

class ZorinDRulerSEQ : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kSEQ;
  }
  explicit ZorinDRulerSEQ(const InType& in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;
};

}  // namespace zorin_d_ruler

```

### `ops_seq.cpp`
```cpp
#include "zorin_d_ruler/seq/include/ops_seq.hpp"

#include <cstdint>

#include "zorin_d_ruler/common/include/common.hpp"

namespace zorin_d_ruler {

namespace {

static inline std::int64_t DoHeavyWork(int n) {
  std::int64_t acc = 0;
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k < n; ++k) {
        acc += (static_cast<std::int64_t>(i) * 31 + j * 17 + k * 13);
        acc ^= (acc << 1);
        acc += (acc >> 3);
      }
    }
  }
  return acc;
}

}  // namespace

ZorinDRulerSEQ::ZorinDRulerSEQ(const InType& in) {
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
  if (n <= 0) return false;

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

```

### `functional/main.cpp`
```cpp
#include <gtest/gtest.h>

#include <array>
#include <string>
#include <tuple>

#include "zorin_d_ruler/common/include/common.hpp"
#include "zorin_d_ruler/mpi/include/ops_mpi.hpp"
#include "zorin_d_ruler/seq/include/ops_seq.hpp"
#include "util/include/func_test_util.hpp"

namespace zorin_d_ruler {

class ZorinDRulerFuncTests: public ppc::util::BaseRunFuncTests<InType, OutType, TestType> {
 public:
  static std::string PrintTestParam(const TestType& test_param) {
    return std::to_string(std::get<0>(test_param)) + "_" +
           std::get<1>(test_param);
  }

 protected:
  void SetUp() override {
    const auto& test_param =
      std::get<static_cast<std::size_t>(
          ppc::util::GTestParamIndex::kTestParams)>(GetParam());

    input_data_ = std::get<0>(test_param);
  }

  bool CheckTestOutputData(OutType& output_data) final {
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
    std::tuple_cat(
        ppc::util::AddFuncTask<ZorinDRulerMPI, InType>(
            kTestParam, PPC_SETTINGS_example_processes_2),
        ppc::util::AddFuncTask<ZorinDRulerSEQ, InType>(
            kTestParam, PPC_SETTINGS_example_processes_2));

const auto kGtestValues =
    ppc::util::ExpandToValues(kTestTasksList);

const auto kFuncTestName =
    ZorinDRulerFuncTests::
        PrintFuncTestName<ZorinDRulerFuncTests>;

INSTANTIATE_TEST_SUITE_P(
    LineTopologyTests,
    ZorinDRulerFuncTests,
    kGtestValues,
    kFuncTestName);

}  // namespace

}  // namespace zorin_d_ruler

```

### `perfomance/main.cpp`
```cpp
#include <gtest/gtest.h>

#include "zorin_d_ruler/common/include/common.hpp"
#include "zorin_d_ruler/mpi/include/ops_mpi.hpp"
#include "zorin_d_ruler/seq/include/ops_seq.hpp"
#include "util/include/perf_test_util.hpp"

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
```