#!/bin/bash
mkdir build-atari
cd build-atari
CC=m68k-atari-mint-gcc-14.3.0 CXX=m68k-atari-mint-g++-14.3.0 cmake .. -DINPUTLEAP_BUILD_GUI=OFF -DINPUTLEAP_BUILD_TESTS=OFF -DINPUTLEAP_BUILD_GULRAK_FILESYSTEM=ON -DINPUTLEAP_BUILD_X11=OFF
