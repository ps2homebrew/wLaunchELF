#!/bin/bash
export SRCDIR="`pwd`"
export BUP_DIR=$PS2DEV/uLE_BUP
export PS2SDKSRC="$PS2DEV/ps2sdksrc"
echo compiling new usbd.irx
cd $PS2DEV/usbd_irx && make clean && make
echo copying usbd.irx into ps2sdk
cp $PS2DEV/usbd_irx/bin/usbd.irx $PS2DEV/ps2sdk/iop/irx
echo script work is now complete
