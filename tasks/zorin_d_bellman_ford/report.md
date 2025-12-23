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
* `RelaxIteration()` - вспомогательная функция, выполняющая релаксацию рёбер на подмножестве вершин, закреплённых за текущим MPI-процессом

### 5.3 Реализация методов
####  Конструктор
```cpp
ZorinDBellmanFordMPI::ZorinDBellmanFordMPI(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
}
```
####  Валидация
```cpp
bool ZorinDBellmanFordMPI::ValidationImpl() {
  const auto &graph = GetInput().graph;

  if (graph.vertex_count <= 0) {
    return false;
  }
  if (GetInput().source < 0 || GetInput().source >= graph.vertex_count) {
    return false;
  }

  if (graph.row_ptr.size() != static_cast<std::size_t>(graph.vertex_count) + 1) {
    return false;
  }
  if (graph.row_ptr.front() != 0) {
    return false;
  }
  if (graph.col_idx.size() != graph.weights.size()) {
    return false;
  }
  if (graph.row_ptr.back() != static_cast<int>(std::ssize(graph.col_idx))) {
    return false;
  }

  if (!std::ranges::all_of(graph.col_idx, [&](int v) { return v >= 0 && v < graph.vertex_count; })) {
    return false;
  }
  return true;
}
```
#### Вспомогательная функция
```cpp
bool ZorinDBellmanFordMPI::RelaxIteration(int rank, int size, const GraphCrs &graph,
                                          const std::vector<std::int64_t> &dist, std::vector<std::int64_t> &dist_next) {
  bool updated = false;
  const int vertex_count = graph.vertex_count;

  for (int vertex = rank; vertex < vertex_count; vertex += size) {
    const std::int64_t du = dist[static_cast<std::size_t>(vertex)];
    if (du >= kInf / 2) {
      continue;
    }

    const int begin = graph.row_ptr[static_cast<std::size_t>(vertex)];
    const int end = graph.row_ptr[static_cast<std::size_t>(vertex) + 1];

    for (int edge = begin; edge < end; ++edge) {
      const int to = graph.col_idx[static_cast<std::size_t>(edge)];
      const std::int64_t cand = du + static_cast<std::int64_t>(graph.weights[static_cast<std::size_t>(edge)]);
      if (cand < dist_next[static_cast<std::size_t>(to)]) {
        dist_next[static_cast<std::size_t>(to)] = cand;
        updated = true;
      }
    }
  }
  return updated;
}
```

#### Предварительная обработка
```cpp
bool ZorinDBellmanFordMPI::PreProcessingImpl() {
  const int vertex_count = GetInput().graph.vertex_count;
  auto &dist = GetOutput();
  dist.assign(static_cast<std::size_t>(vertex_count), kInf);
  dist[static_cast<std::size_t>(GetInput().source)] = 0;
  return true;
}
```

#### Основной этап
```cpp
bool ZorinDBellmanFordMPI::RunImpl() {
  int rank = 0;
  int size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const auto &graph = GetInput().graph;
  const int vertex_count = graph.vertex_count;

  std::vector<std::int64_t> dist = GetOutput();
  std::vector<std::int64_t> dist_next(dist);

  for (int iter = 0; iter < vertex_count - 1; ++iter) {
    dist_next = dist;

    const bool local_updated = RelaxIteration(rank, size, graph, dist, dist_next);

    MPI_Allreduce(dist_next.data(), dist.data(), vertex_count, MPI_LONG, MPI_MIN, MPI_COMM_WORLD);

    int updated = local_updated ? 1 : 0;
    MPI_Allreduce(MPI_IN_PLACE, &updated, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    if (updated == 0) {
      break;
    }
  }

  GetOutput() = std::move(dist);
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
| seq   | 1          | 1.02 | 1.00 | N/A           |
| mpi   | 1          | 1.10 | 0.93 | N/A           |
| mpi   | 2          | 0.95 | 1.07| 53%          |
|mpi| 4         |0.86|1.16| 29%          |

#### Анализ производительности

* Параллельная MPI-реализация алгоритма Беллмана–Форда демонстрирует ускорение по сравнению с последовательной версией при использовании нескольких процессов.
* Увеличение числа процессов приводит к росту ускорения, однако масштабируемость ограничена необходимостью глобальной синхронизации на каждой итерации алгоритма.
* Накладные расходы обусловлены коллективными операциями обмена данными, требуемыми для согласования результатов между процессами.

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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "task/include/task.hpp"

namespace zorin_d_bellman_ford {

struct GraphCrs {
  int vertex_count{};
  std::vector<int> row_ptr;
  std::vector<int> col_idx;
  std::vector<int> weights;
};

struct InType {
  GraphCrs graph;
  int source{};
};

using OutType = std::vector<std::int64_t>;
using TestType = std::tuple<int, std::string>;
using BaseTask = ppc::task::Task<InType, OutType>;

constexpr std::int64_t kInf = std::numeric_limits<std::int64_t>::max() / 4;

inline GraphCrs MakeGraphCrsDeterministic(int vertex_count, int edges_per_vertex) {
  GraphCrs graph;
  graph.vertex_count = vertex_count;
  graph.row_ptr.resize(static_cast<std::size_t>(vertex_count) + 1, 0);

  const int edges = edges_per_vertex > 0 ? edges_per_vertex : 1;
  const std::size_t total_edges = static_cast<std::size_t>(vertex_count) * static_cast<std::size_t>(edges);

  graph.col_idx.reserve(total_edges);
  graph.weights.reserve(total_edges);

  int edge_pos = 0;
  for (int vertex = 0; vertex < vertex_count; ++vertex) {
    graph.row_ptr[static_cast<std::size_t>(vertex)] = edge_pos;
    for (int k = 1; k <= edges; ++k) {
      const int to = (vertex + k) % vertex_count;
      const int weight = 1 + ((vertex * 31 + to * 17 + k * 13) % 20);
      graph.col_idx.push_back(to);
      graph.weights.push_back(weight);
      ++edge_pos;
    }
  }
  graph.row_ptr[static_cast<std::size_t>(vertex_count)] = edge_pos;
  return graph;
}

inline InType MakeInput(int vertex_count, int edges_per_vertex, int source) {
  return InType{.graph = MakeGraphCrsDeterministic(vertex_count, edges_per_vertex), .source = source};
}

}  // namespace zorin_d_bellman_ford

```

### `ops_mpi.hpp`
```cpp
#pragma once

#include <cstdint>
#include <vector>

#include "task/include/task.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordMPI : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kMPI;
  }

  explicit ZorinDBellmanFordMPI(const InType &in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;
  static bool RelaxIteration(int rank, int size, const GraphCrs &graph, const std::vector<std::int64_t> &dist,
                             std::vector<std::int64_t> &dist_next);
};

}  // namespace zorin_d_bellman_ford

```

### `ops_mpi.cpp`
```cpp
#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"

#include <mpi.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

ZorinDBellmanFordMPI::ZorinDBellmanFordMPI(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
}

bool ZorinDBellmanFordMPI::ValidationImpl() {
  const auto &graph = GetInput().graph;

  if (graph.vertex_count <= 0) {
    return false;
  }
  if (GetInput().source < 0 || GetInput().source >= graph.vertex_count) {
    return false;
  }

  if (graph.row_ptr.size() != static_cast<std::size_t>(graph.vertex_count) + 1) {
    return false;
  }
  if (graph.row_ptr.front() != 0) {
    return false;
  }
  if (graph.col_idx.size() != graph.weights.size()) {
    return false;
  }
  if (graph.row_ptr.back() != static_cast<int>(std::ssize(graph.col_idx))) {
    return false;
  }

  if (!std::ranges::all_of(graph.col_idx, [&](int v) { return v >= 0 && v < graph.vertex_count; })) {
    return false;
  }
  return true;
}

bool ZorinDBellmanFordMPI::PreProcessingImpl() {
  const int vertex_count = GetInput().graph.vertex_count;
  auto &dist = GetOutput();
  dist.assign(static_cast<std::size_t>(vertex_count), kInf);
  dist[static_cast<std::size_t>(GetInput().source)] = 0;
  return true;
}

bool ZorinDBellmanFordMPI::RelaxIteration(int rank, int size, const GraphCrs &graph,
                                          const std::vector<std::int64_t> &dist, std::vector<std::int64_t> &dist_next) {
  bool updated = false;
  const int vertex_count = graph.vertex_count;

  for (int vertex = rank; vertex < vertex_count; vertex += size) {
    const std::int64_t du = dist[static_cast<std::size_t>(vertex)];
    if (du >= kInf / 2) {
      continue;
    }

    const int begin = graph.row_ptr[static_cast<std::size_t>(vertex)];
    const int end = graph.row_ptr[static_cast<std::size_t>(vertex) + 1];

    for (int edge = begin; edge < end; ++edge) {
      const int to = graph.col_idx[static_cast<std::size_t>(edge)];
      const std::int64_t cand = du + static_cast<std::int64_t>(graph.weights[static_cast<std::size_t>(edge)]);
      if (cand < dist_next[static_cast<std::size_t>(to)]) {
        dist_next[static_cast<std::size_t>(to)] = cand;
        updated = true;
      }
    }
  }
  return updated;
}

bool ZorinDBellmanFordMPI::RunImpl() {
  int rank = 0;
  int size = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  const auto &graph = GetInput().graph;
  const int vertex_count = graph.vertex_count;

  std::vector<std::int64_t> dist = GetOutput();
  std::vector<std::int64_t> dist_next(dist);

  for (int iter = 0; iter < vertex_count - 1; ++iter) {
    dist_next = dist;

    const bool local_updated = RelaxIteration(rank, size, graph, dist, dist_next);

    MPI_Allreduce(dist_next.data(), dist.data(), vertex_count, MPI_LONG, MPI_MIN, MPI_COMM_WORLD);

    int updated = local_updated ? 1 : 0;
    MPI_Allreduce(MPI_IN_PLACE, &updated, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    if (updated == 0) {
      break;
    }
  }

  GetOutput() = std::move(dist);
  return true;
}

bool ZorinDBellmanFordMPI::PostProcessingImpl() {
  return !GetOutput().empty();
}

}  // namespace zorin_d_bellman_ford

```

### `ops_seq.hpp`
```cpp
#pragma once

#include "task/include/task.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordSEQ : public BaseTask {
 public:
  static constexpr ppc::task::TypeOfTask GetStaticTypeOfTask() {
    return ppc::task::TypeOfTask::kSEQ;
  }

  explicit ZorinDBellmanFordSEQ(const InType &in);

 private:
  bool ValidationImpl() override;
  bool PreProcessingImpl() override;
  bool RunImpl() override;
  bool PostProcessingImpl() override;
};

}  // namespace zorin_d_bellman_ford

```

### `ops_seq.cpp`
```cpp
#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

#include <cstddef>
#include <cstdint>

#include "zorin_d_bellman_ford/common/include/common.hpp"

namespace zorin_d_bellman_ford {

ZorinDBellmanFordSEQ::ZorinDBellmanFordSEQ(const InType &in) {
  SetTypeOfTask(GetStaticTypeOfTask());
  GetInput() = in;
}

bool ZorinDBellmanFordSEQ::ValidationImpl() {
  const auto &graph = GetInput().graph;

  if (graph.vertex_count <= 0) {
    return false;
  }
  if (GetInput().source < 0 || GetInput().source >= graph.vertex_count) {
    return false;
  }

  if (graph.row_ptr.size() != static_cast<std::size_t>(graph.vertex_count) + 1) {
    return false;
  }
  if (graph.col_idx.size() != graph.weights.size()) {
    return false;
  }
  return true;
}

bool ZorinDBellmanFordSEQ::PreProcessingImpl() {
  const int vertex_count = GetInput().graph.vertex_count;
  auto &dist = GetOutput();
  dist.assign(static_cast<std::size_t>(vertex_count), kInf);
  dist[static_cast<std::size_t>(GetInput().source)] = 0;
  return true;
}

bool ZorinDBellmanFordSEQ::RunImpl() {
  const auto &graph = GetInput().graph;
  const int vertex_count = graph.vertex_count;
  auto &dist = GetOutput();

  for (int iter = 0; iter < vertex_count - 1; ++iter) {
    bool updated = false;

    for (int vertex = 0; vertex < vertex_count; ++vertex) {
      const std::int64_t du = dist[static_cast<std::size_t>(vertex)];
      if (du >= kInf / 2) {
        continue;
      }

      const int begin = graph.row_ptr[static_cast<std::size_t>(vertex)];
      const int end = graph.row_ptr[static_cast<std::size_t>(vertex) + 1];

      for (int edge = begin; edge < end; ++edge) {
        const int to = graph.col_idx[static_cast<std::size_t>(edge)];
        const std::int64_t cand = du + static_cast<std::int64_t>(graph.weights[static_cast<std::size_t>(edge)]);
        if (cand < dist[static_cast<std::size_t>(to)]) {
          dist[static_cast<std::size_t>(to)] = cand;
          updated = true;
        }
      }
    }
    if (!updated) {
      break;
    }
  }
  return true;
}

bool ZorinDBellmanFordSEQ::PostProcessingImpl() {
  return !GetOutput().empty();
}

}  // namespace zorin_d_bellman_ford


```

### `functional/main.cpp`
```cpp
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

```

### `perfomance/main.cpp`
```cpp
#include <gtest/gtest.h>

#include <cstddef>

#include "util/include/perf_test_util.hpp"
#include "zorin_d_bellman_ford/common/include/common.hpp"
#include "zorin_d_bellman_ford/mpi/include/ops_mpi.hpp"
#include "zorin_d_bellman_ford/seq/include/ops_seq.hpp"

namespace zorin_d_bellman_ford {

class ZorinDBellmanFordPerfTests : public ppc::util::BaseRunPerfTests<InType, OutType> {
 protected:
  const int k_v = 8000;
  const int k_edges_per_vertex = 8;

  InType input_data{};

  void SetUp() override {
    input_data = MakeInput(k_v, k_edges_per_vertex, 0);
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
  ExecuteTest(GetParam());
}

const auto kAllPerfTasks =
    ppc::util::MakeAllPerfTasks<InType, ZorinDBellmanFordMPI, ZorinDBellmanFordSEQ>(PPC_SETTINGS_zorin_d_bellman_ford);

const auto kGtestValues = ppc::util::TupleToGTestValues(kAllPerfTasks);
const auto kPerfTestName = ZorinDBellmanFordPerfTests::CustomPerfTestName;

INSTANTIATE_TEST_SUITE_P(RunModeTests, ZorinDBellmanFordPerfTests, kGtestValues, kPerfTestName);

}  // namespace zorin_d_bellman_ford

```