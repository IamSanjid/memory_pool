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
Load Average: 1.12, 0.97, 0.82
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
-----------------------------------------------------------------------------------
Benchmark                                         Time             CPU   Iterations
-----------------------------------------------------------------------------------
BM_MemoryPool/iterations:100000               0.005 ms        0.005 ms       100000
BM_MemoryPool/iterations:100000               0.005 ms        0.005 ms       100000
BM_MemoryPool/iterations:100000               0.005 ms        0.005 ms       100000
BM_MemoryPool/iterations:100000               0.005 ms        0.005 ms       100000
BM_MemoryPool/iterations:100000               0.005 ms        0.005 ms       100000
BM_MemoryPool/iterations:100000_mean          0.005 ms        0.005 ms            5
BM_MemoryPool/iterations:100000_median        0.005 ms        0.005 ms            5
BM_MemoryPool/iterations:100000_stddev        0.000 ms        0.000 ms            5
BM_MemoryPool/iterations:100000_cv             0.40 %          0.39 %             5
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000_mean        0.007 ms        0.007 ms            5
BM_ManualMalloc/iterations:100000_median      0.007 ms        0.007 ms            5
BM_ManualMalloc/iterations:100000_stddev      0.000 ms        0.000 ms            5
BM_ManualMalloc/iterations:100000_cv           0.53 %          0.52 %             5
```

# Perf

* Perf1(MemoryPool)
``` 
Performance counter stats for 'build/perf1':

            521.01 msec task-clock                       #    0.997 CPUs utilized          
                51      context-switches                 #   97.887 /sec                   
                 0      cpu-migrations                   #    0.000 /sec                   
               119      page-faults                      #  228.403 /sec                   
     1,367,201,877      cycles                           #    2.624 GHz                      (49.65%)
     2,942,158,569      instructions                     #    2.15  insn per cycle           (62.71%)
       651,755,292      branches                         #    1.251 G/sec                    (63.21%)
         1,559,914      branch-misses                    #    0.24% of all branches          (63.21%)
     1,025,374,411      L1-dcache-loads                  #    1.968 G/sec                    (61.09%)
           126,230      L1-dcache-load-misses            #    0.01% of all L1-dcache accesses  (24.54%)
            37,457      LLC-loads                        #   71.893 K/sec                    (24.51%)
               909      LLC-load-misses                  #    2.43% of all LL-cache accesses  (36.73%)

       0.522504616 seconds time elapsed

       0.521543000 seconds user
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
