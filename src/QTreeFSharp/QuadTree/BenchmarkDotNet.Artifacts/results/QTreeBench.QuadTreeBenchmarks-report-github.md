```

BenchmarkDotNet v0.13.12, Ubuntu 24.04.3 LTS (Noble Numbat)
13th Gen Intel Core i5-13420H, 1 CPU, 12 logical and 8 physical cores
.NET SDK 8.0.127
  [Host]   : .NET 8.0.27 (8.0.2726.22922), X64 RyuJIT AVX2 DEBUG
  ShortRun : .NET 8.0.27 (8.0.2726.22922), X64 RyuJIT AVX2

Job=ShortRun  IterationCount=3  LaunchCount=1  
WarmupCount=3  

```
| Method               | Size | Density | Mean       | Error      | StdDev    | Gen0    | Gen1   | Allocated |
|--------------------- |----- |-------- |-----------:|-----------:|----------:|--------:|-------:|----------:|
| **MatrixMultiply**       | **10**   | **0.05**    |  **34.362 μs** |  **0.5254 μs** | **0.0288 μs** | **10.1929** |      **-** |  **62.51 KB** |
| MatrixVectorMultiply | 10   | 0.05    |   6.331 μs |  0.9732 μs | 0.0533 μs |  2.0752 |      - |  12.73 KB |
| **MatrixMultiply**       | **20**   | **0.05**    | **228.557 μs** | **32.8142 μs** | **1.7987 μs** | **65.6738** | **0.2441** | **403.71 KB** |
| MatrixVectorMultiply | 20   | 0.05    |  19.290 μs |  3.5799 μs | 0.1962 μs |  6.2256 |      - |  38.16 KB |
