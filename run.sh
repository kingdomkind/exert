#!/usr/bin/env bash

#> Exit as soon as an error occurs
set -e

#> Compile & Run
rm -f -- log.txt
git pull
cd build
meson compile 
cd ..

CURRENTDIR=$(pwd)

echo "exec $CURRENTDIR/build/exert" > ~/.xinitrc
startx 2>&1 > log.txt
