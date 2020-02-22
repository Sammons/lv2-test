#!/bin/sh

gcc -std=c++1y -shared -o test.so -fPIC test.c++ -I/usr/include/lv2-c++-tools