#!/usr/bin/env bash

#> Exit as soon as an error occurs
set -e

#> Compile & Run
rm -f -- log.txt
git pull
cd build
meson compile 
cd ..

echo "exec /home/pika/exert/build/exert" > /home/pika/.xinitrc
startx | tee log.txt | less
