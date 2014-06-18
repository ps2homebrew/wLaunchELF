//--------------------------------------------------------------
//File name:    hdd.c
//--------------------------------------------------------------
#include "launchelf.h"

typedef struct{
	char Name[MAX_NAME];
	s64  TotalSize;
	s64  FreeSize;
	s64  UsedSize;
	int  FsGroup;
} PARTYINFO;

enum
{
	CREATE,
	REMOVE,
	RENAME,
	EXPAND,
	FORMAT,
	NUM_MENU
};

PARTYINFO PartyInfo[MAX_ENTRY];
char mountedParty[2][MAX_NAME];
int  numParty;
int  hddSize, hddFree, hddFreeSpace, hddUsed;
int  hddConnected, hddFormated;

//--------------------------------------------------------------
void GetHddInfo(void)
{
	iox_dirent_t infoDirEnt;
	int  rv, hddFd=0, partitionFd, i;
	s64  size=0;
	char tmp[MAX_PATH];
	char dbgtmp[MAX_PATH];
	s64  zoneFree, zoneSize;

	hddSize=0, hddFree=0, hddFreeSpace=0, hddUsed=0;
	hddConnected=0, hddFormated=0;
	numParty=0;
	
	drawMsg("Reading HDD Information...");

	if(hddCheckPresent() < 0){
		hddConnected=0;
		goto end;
	}else
		hddConnected=1;

	if(hddCheckFormatted() < 0){
		hddFormated=0;
		goto end;
	}else
		hddFormated=1;

	hddSize = fileXioDevctl("hdd0:", HDDCTL_TOTAL_SECTORS, NULL, 0, NULL, 0) * 512 / 1024 / 1024;

	if((hddFd = fileXioDopen("hdd0:")) < 0) return;

	rv = fileXioDread(hddFd, &infoDirEnt);
	while(rv > 0)
	{
		if(infoDirEnt.stat.mode != FS_TYPE_EMPTY) hddUsed += infoDirEnt.stat.size / 1024 / 1024;
		rv = fileXioDread(hddFd, &infoDirEnt);
	}
	hddFreeSpace = hddSize - hddUsed;
	hddFree = (hddFreeSpace*100)/hddSize;

	fileXioDclose(hddFd);

//	sprintf(dbgtmp, "HDD Free/Total space = %d/%d", hddFreeSpace, hddSize);
//	drawMsg(dbgtmp);

	if((hddFd = fileXioDopen("hdd0:")) < 0) return;

	while(fileXioDread(hddFd, &infoDirEnt) > 0)
	{
		if(numParty >= MAX_PARTITIONS)
			break;
		if((infoDirEnt.stat.attr & ATTR_SUB_PARTITION) 
				|| (infoDirEnt.stat.mode == FS_TYPE_EMPTY))
			continue;  //Skip subpartition entries and any with FS_TYPE_EMPTY

		sprintf(dbgtmp, "Processing \"%s\"", infoDirEnt.name);
		drawMsg(dbgtmp);

		if(!strncmp(infoDirEnt.name, "__", 2) &&
			strcmp(infoDirEnt.name, "__boot") &&
			strcmp(infoDirEnt.name, "__common"))
			continue;  //Skip names beginning with '__' except for '__boot' and '__common'

		memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
		memset(&PartyInfo[numParty+1], 0, sizeof(PARTYINFO));
		strcpy(PartyInfo[numParty].Name, infoDirEnt.name);

		if((PartyInfo[numParty].Name[0] == '_') && (PartyInfo[numParty].Name[1] == '_'))
			PartyInfo[numParty].FsGroup = FS_GROUP_SYSTEM;
		else if(PartyInfo[numParty].Name[0] == FS_COMMON_PREFIX)
			PartyInfo[numParty].FsGroup = FS_GROUP_COMMON;
		else
			PartyInfo[numParty].FsGroup = FS_GROUP_APPLICATION;

		sprintf(tmp, "hdd0:%s", PartyInfo[numParty].Name);
		partitionFd = fileXioOpen(tmp, O_RDONLY, 0);

//		sprintf(dbgtmp, "Party \"%s\" opened fd=%d", infoDirEnt.name, partitionFd);
//		drawMsg(dbgtmp);

		if(partitionFd < 0){  //if fileXioOpen gave error code ?
			memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
			memset(&PartyInfo[numParty+1], 0, sizeof(PARTYINFO));
			continue;  //Skip this partition as fileXioOpen failed
		}

		for(i = 0, size = 0; i < infoDirEnt.stat.private_0 + 1; i++)
		{
			rv = fileXioIoctl2(partitionFd, HDDIO_GETSIZE, &i, 4, NULL, 0);
			size += rv * 512 / 1024 / 1024;
		}
		PartyInfo[numParty].TotalSize=size;

		fileXioClose(partitionFd);

//		sprintf(dbgtmp, "Party \"%s\" size=%ld", infoDirEnt.name, size);
//		drawMsg(dbgtmp);

		mountParty(tmp);
		zoneFree = fileXioDevctl("pfs0:", PFSCTL_GET_ZONE_FREE, NULL, 0, NULL, 0);
		zoneSize = fileXioDevctl("pfs0:", PFSCTL_GET_ZONE_SIZE, NULL, 0, NULL, 0);
		PartyInfo[numParty].FreeSize  = zoneFree*zoneSize / 1024 / 1024;
		PartyInfo[numParty].UsedSize  = PartyInfo[numParty].TotalSize-PartyInfo[numParty].FreeSize;

		numParty++;

	} //ends while
	fileXioUmount("pfs0:");
	fileXioDclose(hddFd);

end:
	drawMsg("HDD Information Read");

	i=0;
	while(i<50000000) // print operation result during 1 sec
	 i++;

}
//--------------------------------------------------------------
int sizeSelector(int size)
{
	int   x, y;
	int   saveSize=size;
	float scrollBar;
	char  c[MAX_PATH];
	int   event, post_event=0;

	int mSprite_X1 = SCREEN_WIDTH/2-12*FONT_WIDTH;       //Left edge of sprite
	int mSprite_Y1 = (SCREEN_HEIGHT/2-3*FONT_HEIGHT)/2; //Top edge of sprite
	int mSprite_X2 = SCREEN_WIDTH/2+12*FONT_WIDTH;       //Right edge of sprite
	int mSprite_Y2 = (SCREEN_HEIGHT/2+3*FONT_HEIGHT)/2; //Bottom edge of sprite

	event = 1;  //event = initial entry
	while(1){
		//Pad response section
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_RIGHT){
				event |= 2;  //event |= valid pad command
				if((size += 128)>32768)
					size = 32768;
			}else if(new_pad & PAD_LEFT){
				event |= 2;  //event |= valid pad command
				if((size -= 128)<saveSize)
					size = saveSize;
			}else if(new_pad & PAD_R1){
				event |= 2;  //event |= valid pad command
				if(size<1024)
					size=1024;
				else{
					if((size += 1024)>32768)
						size = 32768;
				}
			}else if(new_pad & PAD_L1){
				event |= 2;  //event |= valid pad command
				if((size -= 1024)<saveSize)
					size = saveSize;
			}else if((new_pad & PAD_TRIANGLE)
						|| (!swapKeys && new_pad & PAD_CROSS)
			      || (swapKeys && new_pad & PAD_CIRCLE) ){
				return -1;
			}else if((swapKeys && new_pad & PAD_CROSS)
			      || (!swapKeys && new_pad & PAD_CIRCLE) ){
				event |= 2;  //event |= valid pad command
				break;
			}
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			drawPopSprite(setting->color[0],
				mSprite_X1, mSprite_Y1,
				mSprite_X2, mSprite_Y2);
			drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[1]);

			sprintf(c, "%d MB", size);
			printXY(c, mSprite_X1+12*FONT_WIDTH-(strlen(c)*FONT_WIDTH)/2,
				mSprite_Y1+FONT_HEIGHT/2, setting->color[3], TRUE);
			drawFrame(mSprite_X1+7*FONT_WIDTH, mSprite_Y1+FONT_HEIGHT/4,
				mSprite_X2-7*FONT_WIDTH, mSprite_Y1+FONT_HEIGHT+FONT_HEIGHT/4, setting->color[1]);

			printXY("128MB             32GB", mSprite_X1+FONT_WIDTH,
				mSprite_Y1+FONT_HEIGHT+FONT_HEIGHT/2, setting->color[3], TRUE);

			itoLine(setting->color[1], mSprite_X1+2*FONT_WIDTH+FONT_WIDTH/2,
				mSprite_Y1+2*FONT_HEIGHT+FONT_HEIGHT/2, 0,
				setting->color[1], mSprite_X2-2*FONT_WIDTH-FONT_WIDTH/2,
				mSprite_Y1+2*FONT_HEIGHT+FONT_HEIGHT/2, 0);

			scrollBar = (size*100/32640)*(19*FONT_WIDTH)/100;

			itoLine(setting->color[2], mSprite_X1+2*FONT_WIDTH+FONT_WIDTH/2+(int)scrollBar,
				mSprite_Y1+2*FONT_HEIGHT+FONT_HEIGHT/2-FONT_HEIGHT/4, 0,
				setting->color[2], mSprite_X1+2*FONT_WIDTH+FONT_WIDTH/2+(int)scrollBar,
				mSprite_Y1+2*FONT_HEIGHT+FONT_HEIGHT/2+FONT_HEIGHT/4, 0);
			itoLine(setting->color[2], mSprite_X1+2*FONT_WIDTH+FONT_WIDTH/2+(int)scrollBar-1,
				mSprite_Y1+2*FONT_HEIGHT+FONT_HEIGHT/2-FONT_HEIGHT/4, 0,
				setting->color[2], mSprite_X1+2*FONT_WIDTH+FONT_WIDTH/2+(int)scrollBar-1,
				mSprite_Y1+2*FONT_HEIGHT+FONT_HEIGHT/2+FONT_HEIGHT/4, 0);

			//Tooltip section
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[0],
				0, (y/2),
				SCREEN_WIDTH, y/2+10);
			if (swapKeys)
				printXY("ÿ1:OK ÿ0:Cancel ÿ3:Back Right:+128MB Left:-128MB R1:+1024MB L1:-1024MB", x, y/2, setting->color[2], TRUE);
			else
				printXY("ÿ0:OK ÿ1:Cancel ÿ3:Back Right:+128MB Left:-128MB R1:+1024MB L1:-1024MB", x, y/2, setting->color[2], TRUE);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}//ends while
	return size;
}//ends sizeSelector
//--------------------------------------------------------------
int MenuParty(PARTYINFO *Info)
{
	uint64 color;
	char enable[NUM_MENU], tmp[64];
	int x, y, i, sel;
	int event, post_event=0;

	int menu_ch_w = 8;             //Total characters in longest menu string
	int menu_ch_h = NUM_MENU;      //Total number of menu lines
	int mSprite_Y1 = 32;           //Top edge of sprite
	int mSprite_X2 = SCREEN_WIDTH-35;   //Right edge of sprite
	int mFrame_Y1 = mSprite_Y1;  //Top edge of frame
	int mFrame_X2 = mSprite_X2-3;  //Right edge of frame (-3 correct ???)
	int mFrame_X1 = mFrame_X2-(menu_ch_w+3)*FONT_WIDTH;    //Left edge of frame
	int mFrame_Y2 = mFrame_Y1+(menu_ch_h+1)*FONT_HEIGHT/2; //Bottom edge of frame
	int mSprite_X1 = mFrame_X1-1;  //Left edge of sprite
	int mSprite_Y2 = mFrame_Y2;  //Bottom edge of sprite

	memset(enable, TRUE, NUM_MENU);

	if(Info->FsGroup==FS_GROUP_SYSTEM){
		enable[REMOVE] = FALSE;
		enable[RENAME] = FALSE;
		enable[EXPAND] = FALSE;
	}
	if(Info->FsGroup==FS_GROUP_APPLICATION){
		enable[RENAME] = FALSE;
	}
	if(!strncmp(Info->Name,"PP.HDL.",7)){
		enable[REMOVE] = FALSE;
		enable[RENAME] = FALSE;
		enable[EXPAND] = FALSE;
	}

	for(sel=0; sel<NUM_MENU; sel++)
		if(enable[sel]==TRUE) break;
	
	event = 1;  //event = initial entry
	while(1){
		//Pad response section
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_UP && sel<NUM_MENU){
				event |= 2;  //event |= valid pad command
				do{
					sel--;
					if(sel<0) sel=NUM_MENU-1;
				}while(!enable[sel]);
			}else if(new_pad & PAD_DOWN && sel<NUM_MENU){
				event |= 2;  //event |= valid pad command
				do{
					sel++;
					if(sel==NUM_MENU) sel=0;
				}while(!enable[sel]);
			}else if((new_pad & PAD_TRIANGLE)
						|| (!swapKeys && new_pad & PAD_CROSS)
			      || (swapKeys && new_pad & PAD_CIRCLE) ){
				return -1;
			}else if((swapKeys && new_pad & PAD_CROSS)
			      || (!swapKeys && new_pad & PAD_CIRCLE) ){
				event |= 2;  //event |= valid pad command
				break;
			}
		}

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			drawPopSprite(setting->color[0],
				mSprite_X1, mSprite_Y1,
				mSprite_X2, mSprite_Y2);
			drawFrame(mFrame_X1, mFrame_Y1, mFrame_X2, mFrame_Y2, setting->color[1]);

			for(i=0,y=mFrame_Y1*2+FONT_HEIGHT/2; i<NUM_MENU; i++){
				if(i==CREATE)			strcpy(tmp, "Create");
				else if(i==REMOVE)		strcpy(tmp, "Remove");
				else if(i==RENAME)	strcpy(tmp, "Rename");
				else if(i==EXPAND)	strcpy(tmp, "Expand");
				else if(i==FORMAT)	strcpy(tmp, "Format");

				if(enable[i])	color = setting->color[3];
				else			color = setting->color[1];

				printXY(tmp, mFrame_X1+2*FONT_WIDTH, y/2, color, TRUE);
				y+=FONT_HEIGHT;
			}
			if(sel<NUM_MENU)
				drawChar(127, mFrame_X1+FONT_WIDTH, mFrame_Y1+(FONT_HEIGHT/2+sel*FONT_HEIGHT)/2, setting->color[3]);

			//Tooltip section
			x = SCREEN_MARGIN;
			y = Menu_tooltip_y;
			drawSprite(setting->color[0],
				0, (y/2),
				SCREEN_WIDTH, y/2+10);
			if (swapKeys)
				printXY("ÿ1:OK ÿ0:Cancel ÿ3:Back", x, y/2, setting->color[2], TRUE);
			else
				printXY("ÿ0:OK ÿ1:Cancel ÿ3:Back", x, y/2, setting->color[2], TRUE);
		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}//ends while
	return sel;
}//ends menu
//--------------------------------------------------------------
int CreateParty(char *party, int size)
{
	int  i, num=1, ret=0;
	int  remSize = size-2048;
	char tmpName[MAX_ENTRY];	
	t_hddFilesystem hddFs[MAX_ENTRY];

	drawMsg("Creating New Partition...");

	tmpName[0]=0;
	sprintf(tmpName, "+%s", party);
	for(i=0; i<MAX_ENTRY; i++){
		if(!strcmp(PartyInfo[i].Name, tmpName)){
			sprintf(tmpName, "+%s%d", party, num);
			num++;
		}
	}
	strcpy(party, tmpName+1);
	if(remSize < 0)
		ret = hddMakeFilesystem(size, party, FS_GROUP_COMMON);
	else{
		ret = hddMakeFilesystem(2048, party, FS_GROUP_COMMON);
		hddGetFilesystemList(hddFs, MAX_ENTRY);
		for(i=0; i<MAX_ENTRY; i++){
			if(!strcmp(hddFs[i].name, party))
				ret = hddExpandFilesystem(&hddFs[i], remSize);
		}
	}

	if(ret>0){
		drawMsg("New Partition Created");
	}else{
		drawMsg("Failed Creating New Partition");
	}

	i=0;
	while(i<50000000) // print operation result during 1 sec
	 i++;

	GetHddInfo();

	return ret;
}
//--------------------------------------------------------------
int RemoveParty(char *party)
{
	int  i, ret=0;
	char tmpName[MAX_ENTRY];	
	
	drawMsg("Removing Current Partition...");

	sprintf(tmpName, "hdd0:%s", party);
	ret = fileXioRemove(tmpName);

	if(ret==0){
		drawMsg("Partition Removed");
	}else{
		drawMsg("Failed Removing Current Partition");
	}

	i=0;
	while(i<50000000) // print operation result during 1 sec
	 i++;

	GetHddInfo();

	return ret;
}
//--------------------------------------------------------------
int RenameParty(char *party, char *newName, int fsGroup)
{
	int  i, num=1, ret=0;
	char in[MAX_ENTRY], out[MAX_ENTRY], tmpName[MAX_ENTRY];

	drawMsg("Renaming Partition...");

	if(!strcmp(party, newName))
		goto end;

	in[0]=0;
	out[0]=0;
	tmpName[0]=0;
	if(fsGroup==FS_GROUP_APPLICATION){
		sprintf(tmpName, "%s", newName);
		for(i=0; i<MAX_ENTRY; i++){
			if(!strcmp(PartyInfo[i].Name, tmpName)){
				sprintf(tmpName, "%s%d", newName, num);
				num++;
			}
		}
		strcpy(newName, tmpName);
		sprintf(in, "hdd0:%s", party);
		sprintf(out, "hdd0:%s", newName);
	}else{ // FS_GROUP_COMMON
		sprintf(tmpName, "+%s", newName);
		for(i=0; i<MAX_ENTRY; i++){
			if(!strcmp(PartyInfo[i].Name, tmpName)){
				sprintf(tmpName, "+%s%d", newName, num);
				num++;
			}
		}
		strcpy(newName, tmpName+1);
		sprintf(in, "hdd0:+%s", party);
		sprintf(out, "hdd0:+%s", newName);
	}

	ret = fileXioRename(in, out);

	if(ret==0){
		drawMsg("Partition Renamed");
	}else{
		drawMsg("Failed Renaming Partition");
	}

	i=0;
	while(i<50000000) // print operation result during 1 sec
	 i++;

end:
	GetHddInfo();

	return ret;
}

//--------------------------------------------------------------
int ExpandParty(char *party, int size)
{
	int i, ret=0;
	t_hddFilesystem hddFs[MAX_ENTRY];
	
	drawMsg("Expanding Current Partition...");

	hddGetFilesystemList(hddFs, MAX_ENTRY);
	for(i=0; i<MAX_ENTRY; i++){
		if(!strcmp(hddFs[i].name, party))
			ret = hddExpandFilesystem(&hddFs[i], size);
	}

	if(ret>0){
		drawMsg("Partition Expanded");
	}else{
		drawMsg("Failed Expanding Current Partition");
	}

	i=0;
	while(i<50000000) // print operation result during 1 sec
	 i++;

	GetHddInfo();

	return ret;
}
//--------------------------------------------------------------
int FormatHdd(void)
{
	int i, ret=0;
	
	drawMsg("Formating HDD...");

	ret = hddFormat();

	if(ret==0){
		drawMsg("HDD Formated");
	}else{
		drawMsg("HDD Format Failed");
	}

	i=0;
	while(i<50000000) // print operation result during 1 sec
	 i++;

	GetHddInfo();

	return ret;
}

//--------------------------------------------------------------
void hddManager(void)
{
	char   c[MAX_PATH];
	int    Angle;
	int    x, y, y0, y1, x2, y2;
	float  x3, y3;
	int    i, ret;
	int    partySize;
	int    pfsFree;
	int    ray=50;
	uint64 Color;
	char   tmp[MAX_PATH];
	int    top=0, rows;
	int    event, post_event=0;
	int    browser_sel=0,	browser_nfiles=0;

	rows = (Menu_end_y-Menu_start_y)/FONT_HEIGHT;

	loadHddModules();
	GetHddInfo();

	event = 1;  //event = initial entry
	while(1){

		//Pad response section
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad){
				event |= 2;  //event |= pad command
			}
			if(new_pad & PAD_UP)
				browser_sel--;
			else if(new_pad & PAD_DOWN)
				browser_sel++;
			else if(new_pad & PAD_LEFT)
				browser_sel-=rows/2;
			else if(new_pad & PAD_RIGHT)
				browser_sel+=rows/2;
			else if((new_pad & PAD_SELECT) || (new_pad & PAD_TRIANGLE)){
				if(mountedParty[0][0]!=0) fileXioUmount("pfs0:");
				if(mountedParty[1][0]!=0) fileXioUmount("pfs1:");
				return;
			}else if(new_pad & PAD_R1) {
					ret = MenuParty(&PartyInfo[browser_sel]);
					tmp[0]=0;
					if(ret==CREATE){
						drawMsg("Enter New Partition Name.");
						drawMsg("Enter New Partition Name.");
						if(keyboard(tmp, 36)>0){
							partySize=128;
							drawMsg("Select New Partition Size In MB.");
							drawMsg("Select New Partition Size In MB.");
							if((ret = sizeSelector(partySize))>0){
								if(ynDialog("Create New Partition ?")==1){
									CreateParty(tmp, ret);
									nparties = 0; //Tell FileBrowser to refresh party list
								}
							}
						}
					} else if(ret==REMOVE){
						if(ynDialog("Remove Current Partition ?")==1) {
							RemoveParty(PartyInfo[browser_sel].Name);
							nparties = 0; //Tell FileBrowser to refresh party list
						}	
					} else if(ret==RENAME){
						drawMsg("Enter New Partition Name.");
						drawMsg("Enter New Partition Name.");
						strcpy(tmp, PartyInfo[browser_sel].Name+1);
						if(keyboard(tmp, 36)>0){
							if(ynDialog("Rename Current Partition ?")==1){
								RenameParty(PartyInfo[browser_sel].Name+1, tmp, PartyInfo[browser_sel].FsGroup);
								nparties = 0; //Tell FileBrowser to refresh party list
							}
						}
					} else if(ret==EXPAND){
						drawMsg("Select New Partition Size In MB.");
						drawMsg("Select New Partition Size In MB.");
						partySize=PartyInfo[browser_sel].TotalSize;
						if((ret=sizeSelector(partySize))>0){
							if(ynDialog("Expand Current Partition ?")==1){
								ret -= partySize;
								if(PartyInfo[browser_sel].FsGroup==FS_GROUP_APPLICATION){
									ExpandParty(PartyInfo[browser_sel].Name, ret);
								}else{
									ExpandParty(PartyInfo[browser_sel].Name+1, ret);
								}
								nparties = 0; //Tell FileBrowser to refresh party list
							}
						}
					} else if(ret==FORMAT){
						if(ynDialog("Format HDD ?")==1){
							FormatHdd();
							nparties = 0; //Tell FileBrowser to refresh party list
						}
					}
			}
		}//ends pad response section

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			top=0;
			browser_nfiles=numParty;

			if(top > browser_nfiles-rows)	top=browser_nfiles-rows;
			if(top < 0)				top=0;
			if(browser_sel >= browser_nfiles)		browser_sel=browser_nfiles-1;
			if(browser_sel < 0)				browser_sel=0;
			if(browser_sel >= top+rows)		top=browser_sel-rows+1;
			if(browser_sel < top)			top=browser_sel;
		
			x = Menu_start_x+FONT_WIDTH;
			y = Menu_start_y;

			if(hddConnected==0)
				strcpy(c, "CONNECTED:  NO / FORMATED:  NO");
			else if((hddConnected==1)&&(hddFormated==0))
				strcpy(c, "CONNECTED: YES / FORMATED:  NO");
			else if(hddFormated==1)
				strcpy(c, "CONNECTED: YES / FORMATED: YES");

			printXY("HDD STATUS", x+(strlen(c)/2-strlen("HDD STATUS")/2)*FONT_WIDTH, y/2, setting->color[3], TRUE);

			if(setting->TV_mode == TV_mode_NTSC) 
				y += FONT_HEIGHT+10;
			else
				y += FONT_HEIGHT+11;

			itoLine(setting->color[1], SCREEN_MARGIN, y/2-3, 0,
				setting->color[1], SCREEN_WIDTH/2-30, y/2-3, 0);

			printXY(c, x, y/2, setting->color[3], TRUE);

			itoLine(setting->color[1], SCREEN_WIDTH/2-30, Frame_start_y/2, 0,
				setting->color[1], SCREEN_WIDTH/2-30, Frame_end_y/2, 0);
			itoLine(setting->color[1], SCREEN_WIDTH/2-29, Frame_start_y/2, 0,
				setting->color[1], SCREEN_WIDTH/2-29, Frame_end_y/2, 0);

			x = Menu_start_x+FONT_WIDTH*6;
			if(setting->TV_mode == TV_mode_NTSC) 
				y += FONT_HEIGHT+11;
			else
				y += FONT_HEIGHT+12;

			itoLine(setting->color[1], SCREEN_MARGIN, y/2-4, 0,
				setting->color[1], SCREEN_WIDTH/2-30, y/2-4, 0);

			if(hddFormated==1){

				sprintf(c, "HDD SIZE: %d MB", hddSize);
				printXY(c, x, y/2, setting->color[3], TRUE);
				y += FONT_HEIGHT;
				sprintf(c, "HDD USED: %d MB", hddUsed);
				printXY(c, x, y/2, setting->color[3], TRUE);
				y += FONT_HEIGHT;
				sprintf(c, "HDD FREE: %d MB", hddFreeSpace);
				printXY(c, x, y/2, setting->color[3], TRUE);

				if(setting->TV_mode == TV_mode_NTSC) 
					ray = 45;
				else
					ray = 55;

				x += FONT_WIDTH*9+FONT_WIDTH/2;
				if(setting->TV_mode == TV_mode_NTSC) 
					y += ray+20;
				else
					y += ray+25;

				Angle=0;

				for(i=0; i<360; i++){
					if((Angle = i-90) >= 360) Angle = i+270;
					if(((i*100)/360) >= hddFree)
						Color = setting->color[5];
					else
						Color = setting->color[4];
					x3 = ray*cosdgf(Angle);
					if(setting->TV_mode == TV_mode_NTSC) 
						y3 = (ray-5)*sindgf(Angle);
					else
						y3 = (ray)*sindgf(Angle);
					x2 = x+x3;
					y2 = y/2+(y3/2);
					itoLine(Color, x, y/2, 0, Color, x2, y2, 0);
					itoLine(Color, x, y/2, 0, Color, x2+1, y2, 0);
					itoLine(Color, x, y/2, 0, Color, x2, y2+1, 0);
				}

				sprintf(c, "%d%% FREE",hddFree);
				printXY(c, x-FONT_WIDTH*4, y/2-FONT_HEIGHT/4, setting->color[3], TRUE);

				x = Menu_start_x+FONT_WIDTH*6;
				if(setting->TV_mode == TV_mode_NTSC) 
					y += ray+15;
				else
					y += ray+20;

				itoLine(setting->color[1], SCREEN_MARGIN, y/2-4, 0,
					setting->color[1], SCREEN_WIDTH/2-30, y/2-4, 0);

				sprintf(c, "PFS SIZE: %d MB", (int)PartyInfo[browser_sel].TotalSize);
				printXY(c, x, y/2, setting->color[3], TRUE);
				y += FONT_HEIGHT;
				sprintf(c, "PFS USED: %d MB", (int)PartyInfo[browser_sel].UsedSize);
				printXY(c, x, y/2, setting->color[3], TRUE);
				y += FONT_HEIGHT;
				sprintf(c, "PFS FREE: %d MB", (int)PartyInfo[browser_sel].FreeSize);
				printXY(c, x, y/2, setting->color[3], TRUE);

				pfsFree = (PartyInfo[browser_sel].FreeSize*100)/PartyInfo[browser_sel].TotalSize;

				x += FONT_WIDTH*9+FONT_WIDTH/2;
				if(setting->TV_mode == TV_mode_NTSC) 
					y += ray+20;
				else
					y += ray+25;

				Angle=0;

				for(i=0; i<360; i++){
					if((Angle = i-90) >= 360) Angle = i+270;
					if(((i*100)/360) >= pfsFree)
						Color = setting->color[5];
					else
						Color = setting->color[4];
					x3 = ray*cosdgf(Angle);
					if(setting->TV_mode == TV_mode_NTSC) 
						y3 = (ray-5)*sindgf(Angle);
					else
						y3 = (ray)*sindgf(Angle);
					x2 = x+x3;
					y2 = y/2+(y3/2);
					itoLine(Color, x, y/2, 0, Color, x2, y2, 0);
					itoLine(Color, x, y/2, 0, Color, x2+1, y2, 0);
					itoLine(Color, x, y/2, 0, Color, x2, y2+1, 0);
				}

				sprintf(c, "%d%% FREE",pfsFree);
				printXY(c, x-FONT_WIDTH*4, y/2-FONT_HEIGHT/4, setting->color[3], TRUE);

				rows = (Menu_end_y-Menu_start_y)/FONT_HEIGHT;

				x = SCREEN_WIDTH/2-FONT_WIDTH;
				y = Menu_start_y;

				for(i=0; i<rows; i++)
				{
					if(top+i >= browser_nfiles) break;
					if(top+i == browser_sel) Color = setting->color[2];  //Highlight cursor line
					else			 Color = setting->color[3];

					strcpy(tmp,PartyInfo[top+i].Name);
					printXY(tmp, x+4, y/2, Color, TRUE);
					y += FONT_HEIGHT;
				} //ends for, so all browser rows were fixed above
				if(browser_nfiles > rows) { //if more files than available rows, use scrollbar
					drawFrame(SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-15, Frame_start_y/2,
						SCREEN_WIDTH-SCREEN_MARGIN, Frame_end_y/2, setting->color[1]);
					y0=(Menu_end_y-Menu_start_y+8) * ((double)top/browser_nfiles);
					y1=(Menu_end_y-Menu_start_y+8) * ((double)(top+rows)/browser_nfiles);
					itoSprite(setting->color[1],
						SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-11,
						(y0+Menu_start_y-4)/2,
						SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-1,
						(y1+Menu_start_y-4)/2,
						0);
				} //ends clause for scrollbar
			} //ends hdd formated
			//Tooltip section
			setScrTmp("PS2 HDD MANAGER", "R1:MENU  ÿ3:Exit");

		}//ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;
	}//ends while
	
	return;
}
//--------------------------------------------------------------
