#!/bin/bash
rm -rf build-atari
mkdir build-atari
cd build-atari
#### coldfire
CFLAGS="-mcpu=5475 -DCOLDFIRE" \
CXXFLAGS="-mcpu=5475 -DCOLDFIRE" \
ASMFLAGS="-mcpu=5475 -DCOLDFIRE" \
CC="m68k-atari-mint-gcc-14.3.0" \
CXX="m68k-atari-mint-g++-14.3.0" \
cmake .. -DINPUTLEAP_BUILD_GUI=OFF -DINPUTLEAP_BUILD_TESTS=OFF -DINPUTLEAP_BUILD_GULRAK_FILESYSTEM=ON -DINPUTLEAP_BUILD_X11=OFF && make
#### stock atari
#CC="m68k-atari-mint-gcc-14.3.0" \
#CXX="m68k-atari-mint-g++-14.3.0" \
#cmake .. -DINPUTLEAP_BUILD_GUI=OFF -DINPUTLEAP_BUILD_TESTS=OFF -DINPUTLEAP_BUILD_GULRAK_FILESYSTEM=ON -DINPUTLEAP_BUILD_X11=OFF && make
