#!/bin/bash
export SRCDIR="`pwd`"
export BUP_DIR=$PS2DEV/uLE_BUP
export PS2SDKSRC="$PS2DEV/ps2sdksrc"
echo downloading new source from SVN to uLE_BUP
cd $BUP_DIR
svn co svn://svn.ps2dev.org/ps2/trunk/ps2sdk/iop/usb/usbd/ --revision 1494 usbd_irx
echo deleting old work copy of source
cd $PS2DEV
rm -fr usbd_irx/*
rm -fr usbd_irx/.svn
rmdir usbd_irx
echo copying new work copy of source from uLE_BUP
cp -r uLE_BUP/usbd_irx usbd_irx
echo compiling new usbd.irx
cd $PS2DEV/usbd_irx && make clean && make
echo copying usbd.irx into ps2sdk
cp $PS2DEV/usbd_irx/bin/usbd.irx $PS2DEV/ps2sdk/iop/irx
echo script work is now complete
