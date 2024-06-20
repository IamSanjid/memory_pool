#!/bin/sh
./build_tests.sh && valgrind --leak-check=full --show-leak-kinds=all -s ./build/test -test_all
