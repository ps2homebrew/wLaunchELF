#!/bin/sh
#

source ~/.profile
${PS2SDKSRC:?"Need to set PS2SDKSRC"}
# prepare ps2sdksrc
cp -rf patches/ps2sdk/* $PS2SDKSRC/ || { exit 1; }
## Build and install ps2sdk without FULL_IOMAN
cd $PS2SDKSRC
make clean && make -j 2 && make release && make clean || { exit 1; }
source ~/.profile

# prepare libraries libcdvd, sms and ps2ftpd
# cp -rf oldlibs/* $PS2DEV/
# compiling libraries
# cd $PS2DEV
# making libraries portable again
cd oldlibs
make -C libcdvd clean && make -C libcdvd || { exit 1; }
make -C SMS/drv/SMSMAP clean && make -C SMS/drv/SMSMAP || { exit 1; }
make -C SMS/drv/SMSTCPIP clean && make -C SMS/drv/SMSTCPIP || { exit 1; }
make -C SMS/drv/SMSUTILS clean && make -C SMS/drv/SMSUTILS || { exit 1; }
make -C ps2ftpd clean && make -C ps2ftpd || { exit 1; }
 
