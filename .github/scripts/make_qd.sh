#! /usr/bin/env bash

mkdir build && cd build
cmake -DQD_DEBUG=${QD_DBG} ../
make -j8
