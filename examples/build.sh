#!/bin/bash
cd ../build
make
cd ../examples
gcc functions.c gsc.c -o gsc -I../include -L../build -lgsc -lm
