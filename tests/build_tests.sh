#!/bin/sh

if [ ! -d "./build" ]; then
    mkdir build
fi

g++ -std=c++20 -Wall -Werror -ggdb test.cpp ../fixed_pool.cpp -I../ -o build/test
g++ -std=c++20 -Wall -Werror -ggdb test_obj_moved.cpp ../fixed_pool.cpp -I../ -o build/test_obj_moved
