#!/bin/bash
cd ../build
make
cd ../examples
gcc gsc.c -o gsc -I../include -L../build -lgsc -lm
