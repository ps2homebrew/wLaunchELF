#!/bin/sh
#

# make sure ps2sdk's makefile does not use it
unset PS2SDKSRC

## Download the source code.
rm -rf ps2sdk
git clone git://github.com/ps2dev/ps2sdk.git && cd ps2sdk || exit 1
