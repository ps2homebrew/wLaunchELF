/*      
  _____     ___ ____ 
   ____|   |    ____|      PS2 Open Source Project
  |     ___|   |____       
  
---------------------------------------------------------------------------

    Copyright (C) 2008 - Neme & jimmikaelkael (www.psx-scene.com) 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the Free McBoot License.
    
	This program and any related documentation is provided "as is"
	WITHOUT ANY WARRANTIES, either express or implied, including, but not
 	limited to, implied warranties of fitness for a particular purpose. The
 	entire risk arising out of use or performance of the software remains
 	with you.
   	In no event shall the author be liable for any damages whatsoever
 	(including, without limitation, damages to your hardware or equipment,
 	environmental damage, loss of health, or any kind of pecuniary loss)
 	arising out of the use of or inability to use this software or
 	documentation, even if the author has been advised of the possibility of
 	such damages.

    You should have received a copy of the Free McBoot License along with
    this program; if not, please report at psx-scene :
    http://psx-scene.com/forums/freevast/

---------------------------------------------------------------------------
*/

#include <tamtypes.h>
#include <kernel.h>
#include <libcdvd.h>
#include <string.h>
#include <sifrpc.h>

int Check_ESR_Disc(void)
{
    sceCdRMode ReadMode;
    int result;
    unsigned char SectorBuffer[2112];

    ReadMode.trycount = 5;
    ReadMode.spindlctrl = SCECdSpinNom;
    ReadMode.datapattern = SCECdSecS2048;
    ReadMode.pad = 0;

    result = sceCdReadDVDV(14, 1, SectorBuffer, &ReadMode);  // read LBA 14
    sceCdSync(0);
    if (result != 0) {
        if (sceCdGetError() == SCECdErNO) {
            result = (!strncmp((char *)SectorBuffer + 37, "+NSR", 4)) ? 1 : 0;
        } else
            result = -1;
    } else
        result = -1;

    return result;
}
