#!/bin/bash
export SRCDIR="`pwd`"
export BUP_DIR=$PS2DEV/uLE_BUP
export PS2SDKSRC="$PS2DEV/ps2sdksrc"
echo compiling new usbhdfsd.irx
cd $PS2DEV/usbhdfsd && make clean && make
echo script work is now complete
