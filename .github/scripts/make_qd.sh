#! /usr/bin/env bash

mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=${QD_CXX} -DQD_DEBUG=${QD_DBG} ../
make -j8
