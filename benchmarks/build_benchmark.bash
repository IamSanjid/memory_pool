#!/bin/bash

if [ ! -d "./build" ]; then
    mkdir build
fi

build_google_benchmark() {
    if [ ! -f "./benchmark/build/src/libbenchmark.a" ]; then
        rm -rf benchmark
        if git clone --depth=1 https://github.com/google/benchmark.git; then
            pushd benchmark

            cmake -E make_directory "build"                                       &&  \
            cmake -E chdir "build" cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on         \
                -DBENCHMARK_ENABLE_GTEST_TESTS=OFF -DCMAKE_BUILD_TYPE=Release ../ &&  \
            cmake --build "build" --config Release

            popd
        fi
    fi

    g++ -std=c++20 -Wall -Werror -O3 -DNDEBUG bench_naive.cpp ../fixed_pool.cpp -I../ \
        -isystem benchmark/include -Lbenchmark/build/src -lbenchmark -lpthread -o      \
        build/bench_naive
}

build_google_benchmark

g++ -std=c++20 -Wall -Werror -O3 -DNDEBUG -pg perf1.cpp ../fixed_pool.cpp -I../      \
    -o build/perf1_pg
g++ -std=c++20 -Wall -Werror -O3 -g -DNDEBUG perf1.cpp ../fixed_pool.cpp -I../       \
    -o build/perf1

g++ -std=c++20 -Wall -Werror -O3 -DNDEBUG -pg perf2.cpp ../fixed_pool.cpp -I../      \
    -o build/perf2_pg
g++ -std=c++20 -Wall -Werror -O3 -g -DNDEBUG perf2.cpp ../fixed_pool.cpp -I../       \
    -o build/perf2
