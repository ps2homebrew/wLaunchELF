#!/bin/sh
#

# make sure ps2sdk's makefile does not use it
cd $PS2SDKSRC

## Download the source code.
make clean
git fetch origin &&
git reset --hard origin/master || exit 1
make release && make clean|| { exit 1; }
source ~/.profile