### Результаты BenchmarkDotNet (F#)
Замеры выполнены на микро-размерах из-за архитектурных ограничений рекурсии в эталонной библиотеке.

| Method               | Size | Density | Language | Mean      | Error     | StdDev    | Allocated |
|--------------------- |----- |-------- |----------|----------:|----------:|----------:|----------:|
| MatrixMultiply       | 10   | 0.05    | C        |   1.00 μs | -         | -         |     60 B  |
| MatrixMultiply       | 10   | 0.05    | F#       |  34.36 μs |  0.525 μs |  0.029 μs |  62.51 KB |
| MatrixVectorMultiply | 10   | 0.05    | C        |   1.00 μs | -         | -         |     60 B  |
| MatrixVectorMultiply | 10   | 0.05    | F#       |   6.33 μs |  0.973 μs |  0.053 μs |  12.73 KB |
| MatrixMultiply       | 20   | 0.05    | C        |   8.00 μs | -         | -         |    240 B  |
| MatrixMultiply       | 20   | 0.05    | F#       | 228.56 μs | 32.814 μs |  1.799 μs | 403.71 KB |
| MatrixVectorMultiply | 20   | 0.05    | C        |   2.00 μs | -         | -         |    240 B  |
| MatrixVectorMultiply | 20   | 0.05    | F#       |  19.29 μs |  3.580 μs |  0.196 μs |  38.16 KB |
