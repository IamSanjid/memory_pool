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

# Results
For 100000 iterations: 
```
Run on (4 X 2700 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 256 KiB (x2)
  L3 Unified 3072 KiB (x1)
Load Average: 0.43, 0.66, 0.55
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
-----------------------------------------------------------------------------------
Benchmark                                         Time             CPU   Iterations
-----------------------------------------------------------------------------------
BM_MemoryPool/iterations:100000               0.050 ms        0.012 ms       100000
BM_MemoryPool/iterations:100000               0.071 ms        0.013 ms       100000
BM_MemoryPool/iterations:100000               0.201 ms        0.013 ms       100000
BM_MemoryPool/iterations:100000               0.163 ms        0.014 ms       100000
BM_MemoryPool/iterations:100000                1.17 ms        0.015 ms       100000
BM_MemoryPool/iterations:100000_mean          0.332 ms        0.013 ms            5
BM_MemoryPool/iterations:100000_median        0.163 ms        0.013 ms            5
BM_MemoryPool/iterations:100000_stddev        0.474 ms        0.001 ms            5
BM_MemoryPool/iterations:100000_cv           143.09 %          7.73 %             5
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000             0.007 ms        0.007 ms       100000
BM_ManualMalloc/iterations:100000_mean        0.007 ms        0.007 ms            5
BM_ManualMalloc/iterations:100000_median      0.007 ms        0.007 ms            5
BM_ManualMalloc/iterations:100000_stddev      0.000 ms        0.000 ms            5
BM_ManualMalloc/iterations:100000_cv           2.15 %          1.89 %             5
```

# Perf

* Perf1(MemoryPool)
``` 
Performance counter stats for './build/perf1':

            167.29 msec task-clock                       #    0.992 CPUs utilized          
                31      context-switches                 #  185.305 /sec                   
                 1      cpu-migrations                   #    5.978 /sec                   
            23,674      page-faults                      #  141.513 K/sec                  
       437,442,076      cycles                           #    2.615 GHz                      (52.40%)
       650,342,913      instructions                     #    1.49  insn per cycle           (64.29%)
       130,691,455      branches                         #  781.220 M/sec                    (64.36%)
           241,964      branch-misses                    #    0.19% of all branches          (61.92%)
       213,312,313      L1-dcache-loads                  #    1.275 G/sec                    (54.69%)
         4,302,574      L1-dcache-load-misses            #    2.02% of all L1-dcache accesses  (23.81%)
           864,087      LLC-loads                        #    5.165 M/sec                    (26.15%)
           404,700      LLC-load-misses                  #   46.84% of all LL-cache accesses  (38.54%)

       0.168580286 seconds time elapsed

       0.129973000 seconds user
       0.037734000 seconds sys <-- Need some explanantion why this happens
```

* Perf2(ManualMalloc)
```
Performance counter stats for './build/perf2':

             68.26 msec task-clock                       #    0.990 CPUs utilized          
                13      context-switches                 #  190.449 /sec                   
                 0      cpu-migrations                   #    0.000 /sec                   
               119      page-faults                      #    1.743 K/sec                  
       178,477,208      cycles                           #    2.615 GHz                      (51.60%)
       474,375,593      instructions                     #    2.66  insn per cycle           (65.05%)
       115,416,411      branches                         #    1.691 G/sec                    (64.99%)
           388,056      branch-misses                    #    0.34% of all branches          (64.99%)
       130,574,835      L1-dcache-loads                  #    1.913 G/sec                    (48.44%)
            36,160      L1-dcache-load-misses            #    0.03% of all L1-dcache accesses  (23.32%)
             5,804      LLC-loads                        #   85.028 K/sec                    (23.39%)
               768      LLC-load-misses                  #   13.23% of all LL-cache accesses  (35.02%)

       0.068937789 seconds time elapsed

       0.068823000 seconds user
       0.000000000 seconds sys
```
