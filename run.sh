#!/usr/bin/env bash

#> Exit as soon as an error occurs
set -e
#> Set working directory to script directory
cd "$(dirname "$(realpath "$0")")"

#> Compile & Run
rm -f -- log.txt
git pull
cd build
meson compile 
cd ..

CURRENTDIR=$(pwd)

echo "exec $CURRENTDIR/build/exert" > ~/.xinitrc
startx > log.txt 2>&1
