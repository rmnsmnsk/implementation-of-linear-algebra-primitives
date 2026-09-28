# Реализация примитивов разреженной линейной алгебры (COO формат)

## О проекте

Реализованы базовые операции для разреженных матриц в формате COO (Coordinate format).

## Структура проекта

```
.
├── COO.h          # заголовочный файл
├── COO.c          # реализация операций
├── tests.c        # тесты
└── CMakeLists.txt # сборка
└── benchmark.c
└── matrix.c
└── matrix.h
```

## Формат COO

Разреженная матрица хранится в виде трех массивов:
- `rows_indices` — индексы строк
- `coll_indices` — индексы столбцов
- `values` — значения ненулевых элементов

```c
typedef struct COO {
    int nnz;           // количество ненулевых элементов
    int rows;          // количество строк
    int columns;       // количество столбцов
    int *rows_indices;
    int *coll_indices;
    float *values;
} COO;
```

## Реализованные операции

- `sort_matrix` — сортировка элементов по строкам и столбцам
- `is_line` — проверка, является ли матрица вектором-строкой
- `make_table_vector` — преобразование разреженного вектора в плотный массив
- `multiplication_two_matrix` — умножение двух разреженных матриц
- `multiplication_matrix_and_vector` — умножение матрицы на вектор
- `multiplication_vector_and_matrix` — умножение вектора на матрицу
- `coo_map` — применение функции к каждому элементу
- `coo_map2` — поэлементная бинарная операция над двумя матрицами

## Сборка и запуск

```bash
mkdir build && cd build
cmake ..
make
./tests
```

Или без CMake:

```bash
gcc tests.c COO.c -lm -o tests
./tests
```

## Тестирование

В файле `tests.c` содержатся тесты для всех реализованных функций. При успешном прохождении выводится сообщение:

```
All tests passed
```

## Измерение производительности

Исполняемый файл `benchmark` сравнивает операции умножения для COO и CSparse на 12 матрицах из SuiteSparse Matrix Collection. В набор входят `roadNet-PA`, `roadNet-TX` и `roadNet-CA` размером более миллиона строк и столбцов.

```bash
cd build
./benchmark
```

Для запуска на одной матрице можно передать путь и имя:

```bash
./benchmark ../matrices/roadNet-PA/roadNet-PA.mtx roadNet-PA
```

Итоговые измерения сохранены в `benchmark_results_wsl.txt`, результаты профилирования — в `profiling_raw_wsl.txt` и `profiling_results.csv`. Скрипт `results.py` строит графики по этим файлам.

## Выводы

В ходе работы были реализованы основные примитивы для работы с разреженными матрицами в формате COO. Проведено сравнение производительности с SuiteSparse  (в рамках лабораторной работы).
