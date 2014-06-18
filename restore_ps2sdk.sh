#!/bin/sh
#

# make sure ps2sdk's makefile does not use it
cd $PS2SDKSRC

## Download the source code.
git checkout -f || exit 1
make clean && make -j 2 && make release && make clean|| { exit 1; }
source ~/.profile