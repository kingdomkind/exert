#!/usr/bin/env bash

#> Exit as soon as an error occurs
set -e

#> Compile & Run
cd build
meson compile
cd ..

unset XDG_SEAT
xinit ./xinitrc -- "$XEPHYR" -br -ac -reset -screen 1920x1080 :100