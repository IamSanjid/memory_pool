# Basic Benchmark
./build_benchmark.bash: Need CMake and Git client.

```bash
./build_benchmark.bash
./build/bench_naive --benchmark_time_unit=ms --benchmark_repetitions=5
./build/bench_naive --benchmark_filter=BM_ManualMalloc --benchmark_time_unit=ms --benchmark_repetitions=5
./build/bench_naive --benchmark_filter=BM_MemoryPool --benchmark_time_unit=ms --benchmark_repetitions=5

sudo perf stat -d build/perf1
sudo perf stat -d build/perf2
```

Additional flamegraph generating commands(works better with `-g` gcc flag):
```bash
sudo perf script flamegraph -g --call-graph dwarf ./build/perf1
```

# Results
For 100000 iterations: 
```
Run on (4 X 2700 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 256 KiB (x2)
  L3 Unified 3072 KiB (x1)
Load Average: 0.25, 0.40, 0.58
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
----------------------------------------------------------------------------
Benchmark                                  Time             CPU   Iterations
----------------------------------------------------------------------------
BM_MemoryPool/iterations:100000         5614 ns         5613 ns       100000
BM_ManualMalloc/iterations:100000       6442 ns         6442 ns       100000
user@debian:~/projects/c-cpp/mmap-test/benchmarks$ ./build/bench_naive --benchmark_time_unit=ms --benchmark_repetitions=5
2024-06-22T00:18:44+06:00
Running ./build/bench_naive
Run on (4 X 2700 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 256 KiB (x2)
  L3 Unified 3072 KiB (x1)
Load Average: 0.70, 0.49, 0.60
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
-----------------------------------------------------------------------------------
Benchmark                                         Time             CPU   Iterations
-----------------------------------------------------------------------------------
BM_MemoryPool/iterations:100000               0.006 ms        0.006 ms       100000
BM_MemoryPool/iterations:100000               0.006 ms        0.006 ms       100000
BM_MemoryPool/iterations:100000               0.006 ms        0.006 ms       100000
BM_MemoryPool/iterations:100000               0.006 ms        0.006 ms       100000
BM_MemoryPool/iterations:100000               0.006 ms        0.006 ms       100000
BM_MemoryPool/iterations:100000_mean          0.006 ms        0.006 ms            5
BM_MemoryPool/iterations:100000_median        0.006 ms        0.006 ms            5
BM_MemoryPool/iterations:100000_stddev        0.000 ms        0.000 ms            5
BM_MemoryPool/iterations:100000_cv             2.41 %          2.40 %             5
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.006 ms        0.006 ms       100000
BM_ManualMalloc/iterations:100000             0.006 ms        0.006 ms       100000
BM_ManualMalloc/iterations:100000             0.006 ms        0.006 ms       100000
BM_ManualMalloc/iterations:100000_mean        0.006 ms        0.006 ms            5
BM_ManualMalloc/iterations:100000_median      0.006 ms        0.006 ms            5
BM_ManualMalloc/iterations:100000_stddev      0.000 ms        0.000 ms            5
BM_ManualMalloc/iterations:100000_cv           1.13 %          1.07 %             5
```

# Perf

* Perf1(MemoryPool)
``` 
Performance counter stats for 'build/perf1':

            582.29 msec task-clock                       #    0.999 CPUs utilized          
                 5      context-switches                 #    8.587 /sec                   
                 0      cpu-migrations                   #    0.000 /sec                   
               121      page-faults                      #  207.800 /sec                   
     1,561,331,602      cycles                           #    2.681 GHz                      (50.00%)
     3,148,047,346      instructions                     #    2.02  insn per cycle           (62.91%)
       647,182,981      branches                         #    1.111 G/sec                    (62.91%)
         1,607,837      branch-misses                    #    0.25% of all branches          (62.91%)
       951,728,464      L1-dcache-loads                  #    1.634 G/sec                    (60.99%)
           272,276      L1-dcache-load-misses            #    0.03% of all L1-dcache accesses  (24.73%)
             2,527      LLC-loads                        #    4.340 K/sec                    (24.73%)
               110      LLC-load-misses                  #    4.35% of all LL-cache accesses  (37.09%)

       0.582922377 seconds time elapsed

       0.583210000 seconds user
       0.000000000 seconds sys
```

* Perf2(ManualMalloc)
```
Performance counter stats for 'build/perf2':

            658.12 msec task-clock                       #    0.999 CPUs utilized          
                 5      context-switches                 #    7.597 /sec                   
                 0      cpu-migrations                   #    0.000 /sec                   
               112      page-faults                      #  170.182 /sec                   
     1,731,617,082      cycles                           #    2.631 GHz                      (49.10%)
     4,737,036,002      instructions                     #    2.74  insn per cycle           (61.86%)
     1,152,567,635      branches                         #    1.751 G/sec                    (62.47%)
         4,239,399      branch-misses                    #    0.37% of all branches          (63.08%)
     1,276,198,678      L1-dcache-loads                  #    1.939 G/sec                    (61.84%)
            67,611      L1-dcache-load-misses            #    0.01% of all L1-dcache accesses  (24.76%)
            11,279      LLC-loads                        #   17.138 K/sec                    (24.31%)
               809      LLC-load-misses                  #    7.17% of all LL-cache accesses  (36.46%)

       0.658707000 seconds time elapsed

       0.658773000 seconds user
       0.000000000 seconds sys
```
