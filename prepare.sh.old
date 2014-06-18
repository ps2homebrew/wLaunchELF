#!/bin/sh
#

# make sure ps2sdk's makefile does not use it
unset PS2SDKSRC

## Download the source code.
if test ! -d "ps2sdk"; then
 git clone git://github.com/ps2dev/ps2sdk.git && cd ps2sdk || exit 1
else
 cd ps2sdk &&
 git fetch origin &&
 git reset --hard origin/master || exit 1
fi

cp -rf ../patches/ps2sdk/* ./ || exit 1
 
## Build and install.
make clean && make -j 2 && make release && make clean || { exit 1; }
cd ..

source ~/.profile

if test ! -d "gsKit"; then
 git clone git://github.com/ps2dev/gsKit.git && cd gsKit || exit 1
else
 cd gsKit &&
 git fetch origin &&
 git reset --hard origin/master || exit 1
fi

chmod a+x setup.sh
./setup.sh || { exit 1; }
cd ..
 
cd oldlibs
make -C libjpg clean && make -C libjpg || { exit 1; }
make -C libcdvd clean && make -C libcdvd || { exit 1; }
make -C SMS/drv/SMSMAP clean && make -C SMS/drv/SMSMAP || { exit 1; }
make -C SMS/drv/SMSTCPIP clean && make -C SMS/drv/SMSTCPIP || { exit 1; } //we need $PS2SDKSRC defined
make -C SMS/drv/SMSUTILS clean && make -C SMS/drv/SMSUTILS || { exit 1; }
make -C ps2ftpd clean && make -C ps2ftpd || { exit 1; }
