#!/usr/bin/env bash

#> Exit as soon as an error occurs
set -e

#> Compile & Run
git pull &&
cd build &&
meson compile && 
cd ..

echo "xterm & exec /home/pika/exert/build/exert" > /home/pika/.xinitrc &&
startx > log.txt