#!/bin/bash
export SRCDIR="`pwd`"
export BUP_DIR=$PS2DEV/uLE_BUP
export PS2SDKSRC="$PS2DEV/ps2sdksrc"
echo downloading new source from SVN to uLE_BUP
cd $BUP_DIR
svn co svn://svn.ps2dev.org/ps2/trunk/usbhdfsd --revision 1513 usbhdfsd
echo deleting old work copy of source
cd $PS2DEV
rm -fr usbhdfsd/*
rm -fr usbhdfsd/.svn
rmdir usbhdfsd
echo copying new work copy of source from uLE_BUP
cp -r uLE_BUP/usbhdfsd usbhdfsd
echo updating files changed for uLE
cd "$SRCDIR"
cp -r usbhdfsd/* "$PS2DEV/usbhdfsd"
echo compiling new usbhdfsd.irx
cd $PS2DEV/usbhdfsd && make clean && make
echo script work is now complete
