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
 Performance counter stats for 'build/perf1':

            124.15 msec task-clock                       #    0.995 CPUs utilized          
                 4      context-switches                 #   32.220 /sec                   
                 0      cpu-migrations                   #    0.000 /sec                   
            23,656      page-faults                      #  190.547 K/sec                  
       325,934,217      cycles                           #    2.625 GHz                      (48.42%)
       475,050,346      instructions                     #    1.46  insn per cycle           (61.34%)
        94,406,745      branches                         #  760.436 M/sec                    (61.43%)
           247,020      branch-misses                    #    0.26% of all branches          (61.41%)
       130,411,762      L1-dcache-loads                  #    1.050 G/sec                    (54.79%)
         4,402,810      L1-dcache-load-misses            #    3.38% of all L1-dcache accesses  (25.72%)
           874,413      LLC-loads                        #    7.043 M/sec                    (25.66%)
           522,444      LLC-load-misses                  #   59.75% of all LL-cache accesses  (36.11%)

       0.124822105 seconds time elapsed

       0.056332000 seconds user
       0.068403000 seconds sys <--- Investigation why it spends so much time there
```

* Perf2(ManualMalloc)
```
 Performance counter stats for 'build/perf2':

             68.59 msec task-clock                       #    0.989 CPUs utilized          
                15      context-switches                 #  218.676 /sec                   
                 0      cpu-migrations                   #    0.000 /sec                   
               112      page-faults                      #    1.633 K/sec                  
       176,747,590      cycles                           #    2.577 GHz                      (51.10%)
       468,313,534      instructions                     #    2.65  insn per cycle           (65.16%)
       114,365,419      branches                         #    1.667 G/sec                    (65.21%)
           452,155      branch-misses                    #    0.40% of all branches          (65.15%)
       128,500,900      L1-dcache-loads                  #    1.873 G/sec                    (48.81%)
            18,287      L1-dcache-load-misses            #    0.01% of all L1-dcache accesses  (23.19%)
             4,573      LLC-loads                        #   66.667 K/sec                    (23.22%)
                42      LLC-load-misses                  #    0.92% of all LL-cache accesses  (34.90%)

       0.069380629 seconds time elapsed

       0.069220000 seconds user
       0.000000000 seconds sys
```
