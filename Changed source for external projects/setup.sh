#!/bin/bash
# ulecompile.sh	original by Keith "ubergeek42" Johnson <ubergeek042@gmail.com>
# setup.sh		modifications for a standard ps2dev setup for uLaunchELF by EP

##
## FOR ULAUNCHELF VERSION 4.13
##

# This is a simple script I threw together to download all of the dependencies
# for uLaunchELF.  It will place all the required files into a standard ps2dev
# environment and compile them.

# Usage:
# chmod +x setup.sh and then ./setup.sh, go grab something to eat, and when it
# finishes, you should be able to enter the Source directory, and compile
# uLaunchELF.

# Note:
# If you want to skip the downloading of items, use the -s command, and it
# will only compile the requirements.

#               DO NOT EDIT ANYTHING BELOW THIS LINE!!!!
###########################################################################

##############
## SETTINGS ##
##############
export SRCDIR="`pwd`"
export BUP_DIR=$PS2DEV/uLE_BUP
export PS2SDKSRC="$PS2DEV/ps2sdksrc"

# The -s command line option skips downloading
if [ "$1" = "-s" ]; then
	DOWNLOAD=0
else #otherwise just what they want done
	DOWNLOAD=1
fi

if [ -e $PS2DEV ]; then
  echo PS2DEV folder exists.
  TRUE=1
else
  echo PS2DEV folder is missing, so we must abort
  exit
fi

if [ -e $BUP_DIR ]; then
  echo uLE_BUP folder exists.
  uLE_BUP_NEW=0
else
  echo uLE_BUP folder is missing, so we create it
  mkdir "$BUP_DIR"
  uLE_BUP_NEW=1
  DOWNLOAD=1
fi

####################
## DOWNLOAD FILES ##
####################
if [ $DOWNLOAD -eq 1 ]; then
  cd $BUP_DIR
	svn co svn://svn.ps2dev.org/ps2/trunk/ps2sdk --revision 1663 ps2sdksrc
	svn co svn://svn.ps2dev.org/ps2/trunk/gsKit --revision 1664 gsKit
	svn co svn://svn.ps2dev.org/ps2/trunk/usbhdfsd --revision 1534 usbhdfsd
	svn co svn://svn.ps2dev.org/ps2ware/trunk/ps2ftpd --revision 371 ps2ftpd
	svn co svn://svn.ps2dev.org/ps2ware/trunk/myPS2/lib/libjpg --revision 520 libjpg
	svn co svn://svn.ps2dev.org/ps2ware/trunk/SMS/drv/SMSUTILS --revision 588 SMS/drv/SMSUTILS
	svn co svn://svn.ps2dev.org/ps2ware/trunk/SMS/drv/SMSMAP --revision 588 SMS/drv/SMSMAP
	svn co svn://svn.ps2dev.org/ps2ware/trunk/SMS/drv/SMSTCPIP --revision 588 SMS/drv/SMSTCPIP
if [ -e libcdvd ]; then
  echo libcdvd folder exists.
else
  echo libcdvd folder is missing, so we create it
  mkdir libcdvd
fi
  rm -fr libcdvd/*
	cd libcdvd
	wget http://ps2dev.org/ps2/Projects/Libraries/LIBCDVD:_CD+DVD_Filing_System_for_PS2/LIBCDVD_v1.15:_CD+DVD_Filing_System_for_PS2.download -O libcdvd_1.15.zip
	unzip libcdvd_1.15.zip -d $BUP_DIR/libcdvd
	rm libcdvd_1.15.zip
fi

# Since old work libs are previously patched, and the user wants to recompile,
# we need to erase the old work libs and renew their sources from $BUP_DIR

echo erasing old lib work copies

cd $PS2DEV
rm -fr ps2sdk/*
rmdir ps2sdk
rm -fr ps2sdksrc/*
rm -fr ps2sdksrc/.svn
rmdir ps2sdksrc
rm -fr gsKit/*
rm -fr gsKit/.svn
rm -fr gsKit/.cdtproject
rm -fr gsKit/.project
rmdir gsKit
rm -fr usbhdfsd/*
rm -fr usbhdfsd/.svn
rmdir usbhdfsd
rm -fr ps2ftpd/*
rm -fr ps2ftpd/.svn
rmdir ps2ftpd
rm -fr libjpg/*
rm -fr libjpg/.svn
rmdir libjpg
rm -fr SMS/*
rmdir SMS
rm -fr libcdvd/*
rmdir libcdvd

echo all content of old lib work copies has been erased

echo starting to copy lib work copies from $BUP_DIR

cd $PS2DEV
cp -r uLE_BUP/ps2sdksrc ps2sdksrc
cp -r uLE_BUP/gsKit gsKit
cp -r uLE_BUP/usbhdfsd usbhdfsd
cp -r uLE_BUP/ps2ftpd ps2ftpd
cp -r uLE_BUP/libjpg libjpg
cp -r uLE_BUP/SMS SMS
cp -r uLE_BUP/libcdvd libcdvd

echo lib work copies are now ready for patching

echo we will now apply CSD patches to the work copies of relevant libs

cd "$SRCDIR"
cp -r ps2sdk/* "$PS2DEV/ps2sdksrc"
cp -r gsKit/* "$PS2DEV/gsKit"
cp -r ps2ftpd/* "$PS2DEV/ps2ftpd"
cp -r libjpg/* "$PS2DEV/libjpg"
cp -r libcdvd/* "$PS2DEV/libcdvd"

echo all CSD patches have now been applied

echo it is now time to compile all the libs

cd $PS2DEV/ps2sdksrc && make clean && make && make install
cd $PS2DEV/libjpg && make clean && make
echo now we fix header compatibility to all progs that use libjpg
cd $PS2DEV/libjpg && mkdir include && cp *.h include
cd $PS2DEV/gsKit && make clean && make all && make install
cd $PS2DEV/usbhdfsd && make clean && make
cd $PS2DEV/ps2ftpd && make clean && make
cd $PS2DEV/SMS/drv/SMSUTILS && make clean && make
cd $PS2DEV/SMS/drv/SMSMAP && make clean && make
cd $PS2DEV/SMS/drv/SMSTCPIP && make clean && make
cd $PS2DEV/libcdvd && make clean && make

echo all libs have now been compiled

echo script work is now complete
