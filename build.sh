#!/bin/zsh
clang-9 -std=c++2a -shared -o test.so -fPIC test.c++ -I/usr/include/lv2-c++-tools -lstdc++ -lstdc++fs -pthread

cp test.so test.ttl /usr/lib/lv2/lv2-test.lv2