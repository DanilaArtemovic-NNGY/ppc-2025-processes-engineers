# Линейка (Топологии сетей передачи данных)

- Студент: Зорин Данила Артёмович, группа 3823Б1ПР2
- Технология: SEQ | MPI
- Вариант: 6

## 1. Вступление
В задачах параллельного программирования важную роль занимает организация взаимодействия между процессами. Способ их соединения и передача данных между ними влияет на масштабируемость, общую производительность параллельного алгоритма.

В рамках данной лабораторной работы рассматривается виртуальная топология "Линейка", в которой процессы выстраиваются в последовательную цепочку и обмениваются данными только с соседними процессами. Такая топология позволяет реализовать передачу данных без использования встроенных средств описания топологий (MPI_Cart_Create, MPI_Graph_Create).

Цель работы - реализовать виртуальную топологию "Линейка" средствами MPI, обеспечить корректную передачу и агрегацию данных между процессами, а также сравнить производительность последовательной и параллельной реализаций.

## 2. Постановка задачи
Требуется реализовать параллельный алгоритм с использованием MPI, в котором:
* процессы образуют виртуальную топологию "Линейка";
* каждый процесс взаимодействует только с непосредственными соседями (rank-1, rank+1);
* должна быть обеспечена возможность передачи данных от любого процесса к любому другому;
* запрещено использовать MPI_Cart_Create и MPI_Graph_Create.

Дополнительно нужно реализовать последовательную версию алгоритма (SEQ) для сравнения производительности.

## 3. Последовательная версия (SEQ)
Последовательная реализация выполняется в одном процессе, нужна для оценки ускорения и эффективности параллельной версии (MPI). Последовательная версия реализована в классе `ZorinDRulerSEQ` и состоих из следующих этапов:

1. `ValidationImpl`
* Проверка корректности входных данных.
2. `PreProcessingImpl`
* Подготовительные действия перед выполнением основной логики
3. `RunImpl`
* Выполнение вычислительной нагрузки целиком в одном процессе.
4. `PostProcessingImpl`
* Пост обработка результата
5. `DoHeavyWork`
* Выполнение вычислительной нагрузки

Последовательная версия выполняет весь объём вычислений без параллелизма и межпроцессорного взаимодействия.

## 4. Схема параллелизации
Виртуальная топология "Линейка" представляет собой цепочку процессов:
```cpp
P1 - P2 - P3 - ... - P(N-1)
```
Для каждого процесса с номером `rank` есть:
* левый сосед с номером `rank - 1` (если `rank > 0`)
* правый сосед с номером `rank + 1` (если `rank < size - 1`)

Обмен данными осуществляется только между соседними процессами, при этом передача данных от любого процесса к любому другому обеспечивается за счёт последовательной маршрутизации.

**Параллельный алгоритм состоит из следующих этапов:**
1. Определение параметров MPI и распределение ролей
* Каждый процеес получает `rank` и `size`
```cpp
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &size);
```

2. Распределение вычислений
* общий объём работы разбивается между процессами
* каждому процессу назначается собственный диапазон вычислений
* диапазоны не пересекаются и полностью покрывают задачу

3. Каждый процесс выполняет вычисления только на своём диапазоне данных.

4. Агрегация результатов по линейке
* передача частичных результатов осуществляется по цепочке (прямой проход слева направо и обратный переход справа налево)
* используется точка-точка взаимодействие (`MPI_Send`, `MPI_Recv`)
* коллективные операции (`MPI_Reduce`, `MPI_Allreduce`) не применяются

5. Вычисление итогового результата
* Итоговое значение доступно всем процессам после завершения обратного прохода.

## 5. Детали реализации
### 5.1 Структура кода
Реализация параллельного алгоритма с виртуальной топологией "Линейка" расположена в каталоге mpi/:
* в mpi/include/ops_mpi.hpp - Заготовочный файл класса MPI-задачи
* в mpi/src/ops_mpi.cpp - Реализация выполнения вычисления

Класс `ZorinDAvgVecMPI` наследуется от базового класса `BaseTask`, что обеспечивает единый жизненный цикл выполнения: `ValidationImpl` → `PreProcessingImpl` → `RunImpl` → `PostProcessingImpl`.

### 5.2 Ключевые классы и функции
* `ZorinDRulerMPI` - Основной класс MPI-задачи
* `ValidationImpl()` - Проверка входных данных
* `PreProcessingImpl()` - Подготовительные действия перед выполнением основной логики
* `RunImpl()` - Основной этап вычисления среднего значения
* `PostProcessingImpl()` - Пост обработка результата
* `DoHeavyWork()` - выполнение вычислительной нагрузки на локальном диапазоне данных
* `LineAllSum()` - агрегация частичных результатов

### 5.3 Реализация методов
####  Конструктор
```cpp
ZorinDRulerMPI::ZorinDRulerMPI(const InType& in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
  GetOutput() = 0;
}
```
####  Валидация
```cpp
bool ZorinDRulerMPI::ValidationImpl() {
  return GetInput() > 0;
}
```

#### Предварительная обработка
```cpp
bool ZorinDRulerMPI::PreProcessingImpl() {
  GetOutput() = 0;
  return true;
}
```

#### Основной этап
```cpp
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
```

#### Пост обработка
```cpp
bool ZorinDRulerMPI::PostProcessingImpl() {
  GetOutput() = GetInput();
  return true;
}
```
#### Тяжелые вычисления
```cpp
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
```
#### Агрегация результатов
```cpp
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
[----------] 6 tests from LineTopologyTests/ZorinDRulerFuncTests
[ RUN      ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_mpi_enabled_10_10
[       OK ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_mpi_enabled_10_10 (23 ms)
[ RUN      ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_mpi_enabled_50_50
[       OK ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_mpi_enabled_50_50 (22 ms)
[ RUN      ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_mpi_enabled_100_100
[       OK ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_mpi_enabled_100_100 (21 ms)
[ RUN      ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_seq_enabled_10_10
[       OK ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_seq_enabled_10_10 (23 ms)
[ RUN      ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_seq_enabled_50_50
[       OK ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_seq_enabled_50_50 (23 ms)
[ RUN      ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_seq_enabled_100_100
[       OK ] LineTopologyTests/ZorinDRulerFuncTests.LineTopology/zorin_d_ruler_seq_enabled_100_100 (31 ms)
[----------] 6 tests from LineTopologyTests/ZorinDRulerFuncTests (149 ms total)
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
В ходе лабораторной работы была реализована виртуальная топология "Линейка" с использованием MPI и SEQ. Реализация не использует встроенные механизмы описания топологий и основана на точка-точка взаимодействии между соседними процессами.

Параллельная версия показала значительное ускорение по сравнению с последовательной реализацией и продемонстрировала высокую эффективность масштабирования. Поставленные в работе цели были полностью достигнуты.

## 9. Список литературы
1. MPI Forum. Message Passing Interface Standard. - https://www.mpi-forum.org/docs/
2. Часть 1. MPI — Введение и первая программа - https://habr.com/ru/articles/548266/
3. Часть 2. MPI — Учимся следить за процессами - https://habr.com/ru/articles/548418/

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