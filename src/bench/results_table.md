# Benchmark Results

| Dataset | Operation | C (time) | F# (time) | Note |
|---------|-----------|----------|-----------|--------|
| cage4 | mm | 37.0 µs | 557.0 µs | OK |
| curtis54 | mm | 2.9 ms | 14.6 ms | OK |
| lp_afiro | mm | 227.0 µs | 7.9 ms | OK |
| Synthetic 100×100 | mm | 2.5 ms | 225.3 ms | OK |
| Synthetic 500×500 | mm | 520.6 ms | 13.91 s | OK |
| Synthetic 1000×1000 | mm | 2.22 s | 62.17 s | OK |
| Synthetic 2000×2000 | mm | 9.24 s | N/A | Timeout / Stack Overflow |
| cage4 | mv | 2.0 µs | 80.0 µs | OK |
| curtis54 | mv | 6.0 µs | 474.0 µs | OK |
| lp_afiro | mv | 3.0 µs | 18.0 µs | OK |
| Synthetic 100×100 | mv | 2.0 µs | 4.0 ms | OK |
| Synthetic 500×500 | mv | 11.3 µs | N/A | Timeout / Stack Overflow |
| Synthetic 1000×1000 | mv | 36.0 µs | N/A | Timeout / Stack Overflow |
| Synthetic 2000×2000 | mv | 51.3 µs | N/A | Timeout / Stack Overflow |