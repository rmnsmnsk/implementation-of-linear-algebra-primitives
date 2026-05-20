# Benchmark Results

| Size | Density | Operation | C (time) | F# (time) | Note |
|------|---------|-----------|----------|-----------|--------|
| 100×100 | 0.05 | mm | 2.5 ms | 225.3 ms | OK |
| 500×500 | 0.02 | mm | 520.6 ms | 13.91 s | OK |
| 1000×1000 | 0.01 | mm | 2.22 s | 62.17 s | OK |
| 2000×2000 | 0.005 | mm | 9.24 s | N/A | Timeout / Stack Overflow |
| 100×100 | 0.05 | mv | 2.0 µs | 4.0 ms | OK |
| 500×500 | 0.02 | mv | 11.3 µs | N/A | Timeout / Stack Overflow |
| 1000×1000 | 0.01 | mv | 36.0 µs | N/A | Timeout / Stack Overflow |
| 2000×2000 | 0.005 | mv | 51.3 µs | N/A | Timeout / Stack Overflow |