#!/bin/bash
export SRCDIR="`pwd`"
export BUP_DIR=$PS2DEV/uLE_BUP
export PS2SDKSRC="$PS2DEV/ps2sdksrc"
echo downloading new source from SVN to uLE_BUP
cd $BUP_DIR
svn co svn://svn.ps2dev.org/ps2ware/trunk/ps2ftpd --revision 371 ps2ftpd
echo deleting old work copy of source
cd $PS2DEV
rm -fr ps2ftpd/*
rm -fr ps2ftpd/.svn
rmdir ps2ftpd
echo copying new work copy of source from uLE_BUP
cp -r uLE_BUP/ps2ftpd ps2ftpd
echo applying CSD patches to work copy of source
cd "$SRCDIR"
cp -r ps2ftpd/* "$PS2DEV/ps2ftpd"
echo compiling new binary
cd $PS2DEV/ps2ftpd && make clean && make
echo script work is now complete



