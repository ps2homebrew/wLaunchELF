//--------------------------------------------------------------
//File name:    hdd.c
//--------------------------------------------------------------
#include "launchelf.h"

typedef struct{
	char Name[MAX_NAME];
	s64  RawSize;
	s64  TotalSize;
	s64  FreeSize;
	s64  UsedSize;
	int  FsGroup;
	//GameInfo Game;  //Intended for HDL game info, but not yet implemented
	int  Count;
	int  Treatment;
} PARTYINFO;

enum {//For Treatment codes
	TREAT_PFS = 0,			//PFS partition for normal file access
	TREAT_HDL,			//HDL game partition
	TREAT_SYSTEM,		//System partition that must never be modified (__mbr)
	TREAT_NOACCESS  //Inaccessible partition (but can be deleted)
};

enum {//For menu commands
	CREATE = 0,
	REMOVE,
	RENAME,
	EXPAND,
	FORMAT,
	NUM_MENU
};

#define SECTORS_PER_MB 2048  //Divide by this to convert from sector count to MB
#define MB 1048576

PARTYINFO PartyInfo[MAX_PARTITIONS];
int  numParty;
int  hddSize, hddFree, hddFreeSpace, hddUsed;
int  hddConnected, hddFormated;

//--------------------------------------------------------------
/*
void DebugDispStat(iox_dirent_t *p)
{
	char buf[MAX_TEXT_LINE*24]="";
	int  done, stp, pos, i;
	unsigned int *ip = &p->stat.private_0;
	int *cp = (int *) p->stat.ctime;
	int *ap = (int *) p->stat.atime;
	int *mp = (int *) p->stat.mtime;

	for(done=0; done==0;){
		pos = 0;
		sprintf(buf+pos,"dirent.name == \"%s\"\n%n", p->name, &stp); pos+=stp;
		sprintf(buf+pos,"dirent.unknown == %d\n%n", p->unknown, &stp); pos+=stp;
		sprintf(buf+pos,"  mode == 0x%08X, attr == 0x%08X\n%n",p->stat.mode,p->stat.attr,&stp); pos+=stp;
		sprintf(buf+pos,"hisize == 0x%08X, size == 0x%08X\n%n",p->stat.hisize,p->stat.size,&stp); pos+=stp;
		sprintf(buf+pos,"ctime == 0x%08X%08X                      \n%n", cp[1],cp[0], &stp); pos+=stp;
		sprintf(buf+pos,"atime == 0x%08X%08X\n%n", ap[1],ap[0], &stp); pos+=stp;
		sprintf(buf+pos,"mtime == 0x%08X%08X\n%n", mp[1],mp[0], &stp); pos+=stp;
		for(i=0; i<6; i++){
			sprintf(buf+pos,"private_%d == 0x%08X\n%n", i, ip[i], &stp); pos+=stp;
		}
		nonDialog(buf);
		drawLastMsg();
		if(readpad() && new_pad){
			done=1;
		}
	}
}
*/
//------------------------------
//endfunc DebugDispStat
//--------------------------------------------------------------
void GetHddInfo(void)
{
	iox_dirent_t infoDirEnt;
	int  rv, hddFd=0, partitionFd, i, Treat;
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

	size =((s64) fileXioDevctl("hdd0:", HDDCTL_TOTAL_SECTORS, NULL, 0, NULL, 0))*512;
	hddSize = (size / MB);

	if((hddFd = fileXioDopen("hdd0:")) < 0) return;

	while(fileXioDread(hddFd, &infoDirEnt) > 0){ //Starts main while loop
		int	found_size =
					((((s64) infoDirEnt.stat.hisize)<<32) + infoDirEnt.stat.size) / MB;

		//DebugDispStat(&infoDirEnt);

		if(infoDirEnt.stat.mode == FS_TYPE_EMPTY)
			continue;
		hddUsed += found_size;
		for(i=0; i<numParty; i++) //Check with previous partitions
			if(!strcmp(infoDirEnt.name,PartyInfo[i].Name))
				break;
		if(i<numParty) { //found another reference to an old name
			PartyInfo[i].RawSize += found_size; //Add new segment to old size
		} else { //Starts clause for finding brand new name for PartyInfo[numParty]
			sprintf(dbgtmp, "Found \"%s\"", infoDirEnt.name);
			drawMsg(dbgtmp);

			memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
			strcpy(PartyInfo[numParty].Name, infoDirEnt.name);
			PartyInfo[numParty].RawSize = found_size; //Store found segment size
			PartyInfo[numParty].Count = numParty;

			if(infoDirEnt.stat.mode == 0x0001)  //New test of partition type by 'mode'
				Treat = TREAT_SYSTEM;
			else if(!strncmp(infoDirEnt.name,"PP.HDL.",7))
				Treat = TREAT_HDL;
			else {
				sprintf(tmp, "hdd0:%s", infoDirEnt.name);
				partitionFd = fileXioOpen(tmp, O_RDONLY, 0);
				if(partitionFd < 0){
					Treat = TREAT_NOACCESS; //needed for Sony-style protected partitions
				} else {
					fileXioClose(partitionFd);
					Treat = TREAT_PFS;
				}
			}
			PartyInfo[numParty].Treatment = Treat; 

			if((PartyInfo[numParty].Name[0] == '_') && (PartyInfo[numParty].Name[1] == '_'))
				PartyInfo[numParty].FsGroup = FS_GROUP_SYSTEM;
			else if(PartyInfo[numParty].Name[0] == FS_COMMON_PREFIX)
				PartyInfo[numParty].FsGroup = FS_GROUP_COMMON;
			else
				PartyInfo[numParty].FsGroup = FS_GROUP_APPLICATION;

			if(Treat == TREAT_PFS){ //Starts clause for TREAT_PFS
				sprintf(tmp, "hdd0:%s", PartyInfo[numParty].Name);
				partitionFd = fileXioOpen(tmp, O_RDONLY, 0);

				for(i = 0, size = 0; i < infoDirEnt.stat.private_0 + 1; i++)
				{
					rv = fileXioIoctl2(partitionFd, HDDIO_GETSIZE, &i, 4, NULL, 0);
					size += rv * 512 / 1024 / 1024;
				}
				PartyInfo[numParty].TotalSize=size;
	
				fileXioClose(partitionFd);
	
				mountParty(tmp);
				zoneFree = fileXioDevctl("pfs0:", PFSCTL_GET_ZONE_FREE, NULL, 0, NULL, 0);
				zoneSize = fileXioDevctl("pfs0:", PFSCTL_GET_ZONE_SIZE, NULL, 0, NULL, 0);
				PartyInfo[numParty].FreeSize  = zoneFree*zoneSize / MB;
				PartyInfo[numParty].UsedSize  = PartyInfo[numParty].TotalSize-PartyInfo[numParty].FreeSize;
		
			} //Ends clause for TREAT_PFS

			numParty++;
		} //Ends clause for finding brand new name for PartyInfo[numParty]
	} //ends main while loop
	fileXioUmount("pfs0:");
	mountedParty[0][0]=0;
	fileXioDclose(hddFd);
	hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80; //free space rounded to useful area
	hddFree = (hddFreeSpace*100)/hddSize;            //free space percentage

end:
	drawMsg("HDD Information Read");

	i=0;
	while(i<25000000) // print operation result during 1 sec
	 i++;

} // ends GetHddInfo
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
int MenuParty(PARTYINFO Info)
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

	if(Info.FsGroup==FS_GROUP_SYSTEM){
		enable[REMOVE] = FALSE;
		enable[RENAME] = FALSE;
		enable[EXPAND] = FALSE;
	}
	if(Info.FsGroup==FS_GROUP_APPLICATION){
		enable[RENAME] = FALSE;
	}
	if(!strncmp(Info.Name,"PP.HDL.",7)){
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
	t_hddFilesystem hddFs[MAX_PARTITIONS];

	drawMsg("Creating New Partition...");

	tmpName[0]=0;
	sprintf(tmpName, "+%s", party);
	for(i=0; i<MAX_PARTITIONS; i++){
		if(!strcmp(PartyInfo[i].Name, tmpName)){
			sprintf(tmpName, "+%s%d", party, num);
			num++;
		}
	}
	strcpy(party, tmpName+1);
	if(remSize <= 0)
		ret = hddMakeFilesystem(size, party, FS_GROUP_COMMON);
	else{
		ret = hddMakeFilesystem(2048, party, FS_GROUP_COMMON);
		hddGetFilesystemList(hddFs, MAX_PARTITIONS);
		for(i=0; i<MAX_PARTITIONS; i++){
			if(!strcmp(hddFs[i].name, party))
				ret = hddExpandFilesystem(&hddFs[i], remSize);
		}
	}

	if(ret>0){
		GetHddInfo();
		drawMsg("New Partition Created");
	}else{
		drawMsg("Failed Creating New Partition");
	}

	i=0;
	while(i<25000000) // print operation result during 1 sec
	 i++;

	return ret;
}
//--------------------------------------------------------------
int RemoveParty(PARTYINFO Info)
{
	int  i, ret=0;
	char tmpName[MAX_ENTRY];	
	
	//printf("Remove Partition: %d\n", Info.Count);

	drawMsg("Removing Current Partition...");

	sprintf(tmpName, "hdd0:%s", Info.Name);
	ret = fileXioRemove(tmpName);

	if(ret==0){
		hddUsed -= Info.TotalSize;
		hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80; //free space rounded to useful area
		hddFree = (hddFreeSpace*100)/hddSize;            //free space percentage
		numParty--;
		if(Info.Count==numParty){
			memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
		}else{
			for(i=Info.Count; i<numParty; i++){
				memset(&PartyInfo[i], 0, sizeof(PARTYINFO));
				memcpy(&PartyInfo[i], &PartyInfo[i+1], sizeof(PARTYINFO));
				PartyInfo[i].Count--;
			}
			memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
		}
		drawMsg("Partition Removed");
	}else{
		drawMsg("Failed Removing Current Partition");
	}

	i=0;
	while(i<25000000) // print operation result during 1 sec
	 i++;

	return ret;
}
//--------------------------------------------------------------
int RenameParty(PARTYINFO Info, char *newName)
{
	int  i, num=1, ret=0;
	char in[MAX_ENTRY], out[MAX_ENTRY], tmpName[MAX_ENTRY];

	//printf("Rename Partition: %d  group: %d\n", Info.Count, Info.FsGroup);
	drawMsg("Renaming Partition...");

	in[0]=0;
	out[0]=0;
	tmpName[0]=0;
	if(Info.FsGroup==FS_GROUP_APPLICATION){
		sprintf(tmpName, "%s", newName);
		if(!strcmp(Info.Name, tmpName))
			goto end;
		for(i=0; i<MAX_PARTITIONS; i++){
			if(!strcmp(PartyInfo[i].Name, tmpName)){
				sprintf(tmpName, "%s%d", newName, num);
				num++;
			}
		}
		strcpy(newName, tmpName);
		sprintf(in, "hdd0:%s", Info.Name);
		sprintf(out, "hdd0:%s", newName);
	}else{ // FS_GROUP_COMMON
		sprintf(tmpName, "+%s", newName);
		if(!strcmp(Info.Name, tmpName))
			goto end;
		for(i=0; i<MAX_PARTITIONS; i++){
			if(!strcmp(PartyInfo[i].Name, tmpName)){
				sprintf(tmpName, "+%s%d", newName, num);
				num++;
			}
		}
		strcpy(newName, tmpName+1);
		sprintf(in, "hdd0:%s", Info.Name);
		sprintf(out, "hdd0:+%s", newName);
	}

	ret = fileXioRename(in, out);

	if(ret==0){
		if(Info.FsGroup==FS_GROUP_APPLICATION)
			strcpy(PartyInfo[Info.Count].Name, newName);
		else // FS_GROUP_COMMON
			sprintf(PartyInfo[Info.Count].Name, "+%s", newName);
		drawMsg("Partition Renamed");
	}else{
		drawMsg("Failed Renaming Partition");
	}

	i=0;
	while(i<25000000) // print operation result during 1 sec
	 i++;

end:
	return ret;
}

//--------------------------------------------------------------
int ExpandParty(PARTYINFO Info, int size)
{
	int i, ret=0;
	char tmpName[MAX_ENTRY];
	t_hddFilesystem hddFs[MAX_PARTITIONS];
	
	drawMsg("Expanding Current Partition...");
	//printf("Expand Partition: %d\n", Info.Count);

	if(Info.FsGroup==FS_GROUP_APPLICATION){
		strcpy(tmpName, Info.Name);
	}else{
		strcpy(tmpName, Info.Name+1);
	}

	hddGetFilesystemList(hddFs, MAX_PARTITIONS);
	for(i=0; i<MAX_PARTITIONS; i++){
		if(!strcmp(hddFs[i].name, tmpName))
			ret = hddExpandFilesystem(&hddFs[i], size);
	}

	if(ret>0){
		hddUsed += size;
		hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80; //free space rounded to useful area
		hddFree = (hddFreeSpace*100)/hddSize;            //free space percentage

		PartyInfo[Info.Count].TotalSize += size;
		PartyInfo[Info.Count].FreeSize += size;
		drawMsg("Partition Expanded");
	}else{
		drawMsg("Failed Expanding Current Partition");
	}

	i=0;
	while(i<25000000) // print operation result during 1 sec
	 i++;

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
	while(i<25000000) // print operation result during 1 sec
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
	int    Treat;

	rows = (Menu_end_y-Menu_start_y)/FONT_HEIGHT;

	loadHddModules();
	GetHddInfo();

	event = 1;  //event = initial entry
	while(1){

		//Pad response section
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad){
				//printf("Selected Partition: %d\n", browser_sel);
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
				fileXioUmount("pfs0:");
				mountedParty[0][0]=0;
				fileXioUmount("pfs1:");
				mountedParty[1][0]=0;
				return;
			}else if(new_pad & PAD_R1) { //Starts clause for R1 menu
				ret = MenuParty(PartyInfo[browser_sel]);
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
						RemoveParty(PartyInfo[browser_sel]);
						nparties = 0; //Tell FileBrowser to refresh party list
					}	
				} else if(ret==RENAME){
					drawMsg("Enter New Partition Name.");
					drawMsg("Enter New Partition Name.");
					strcpy(tmp, PartyInfo[browser_sel].Name+1);
					if(keyboard(tmp, 36)>0){
						if(ynDialog("Rename Current Partition ?")==1){
							RenameParty(PartyInfo[browser_sel], tmp);
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
							ExpandParty(PartyInfo[browser_sel], ret);
							nparties = 0; //Tell FileBrowser to refresh party list
						}
					}
				} else if(ret==FORMAT){
					if(ynDialog("Format HDD ?")==1){
						FormatHdd();
						nparties = 0; //Tell FileBrowser to refresh party list
					}
				}
			} //Ends clause for R1 menu
		}//ends pad response section

		if(event||post_event){ //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[0]);

			browser_nfiles=numParty;
			//printf("Number Of Partition: %d\n", numParty);

			if(top > browser_nfiles-rows)	top=browser_nfiles-rows;
			if(top < 0)				top=0;
			if(browser_sel >= browser_nfiles)		browser_sel=browser_nfiles-1;
			if(browser_sel < 0)				browser_sel=0;
			if(browser_sel >= top+rows)		top=browser_sel-rows+1;
			if(browser_sel < top)			top=browser_sel;
		
			y = Menu_start_y;

			x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x)-(strlen("HDD STATUS")*FONT_WIDTH)/2;
			printXY("HDD STATUS", x, y/2, setting->color[3], TRUE);

			if(setting->TV_mode == TV_mode_NTSC) 
				y += FONT_HEIGHT+10;
			else
				y += FONT_HEIGHT+11;

			itoLine(setting->color[1], SCREEN_MARGIN, y/2-3, 0,
				setting->color[1], SCREEN_WIDTH/2-20, y/2-3, 0);

			if(hddConnected==0)
				strcpy(c, "CONNECTED:  NO / FORMATED:  NO");
			else if((hddConnected==1)&&(hddFormated==0))
				strcpy(c, "CONNECTED: YES / FORMATED:  NO");
			else if(hddFormated==1)
				strcpy(c, "CONNECTED: YES / FORMATED: YES");

			x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x)-(strlen(c)*FONT_WIDTH)/2;
			printXY(c, x, y/2, setting->color[3], TRUE);

			itoLine(setting->color[1], SCREEN_WIDTH/2-20, Frame_start_y/2, 0,
				setting->color[1], SCREEN_WIDTH/2-20, Frame_end_y/2, 0);
			itoLine(setting->color[1], SCREEN_WIDTH/2-21, Frame_start_y/2, 0,
				setting->color[1], SCREEN_WIDTH/2-21, Frame_end_y/2, 0);

			if(setting->TV_mode == TV_mode_NTSC) 
				y += FONT_HEIGHT+11;
			else
				y += FONT_HEIGHT+12;

			itoLine(setting->color[1], SCREEN_MARGIN, y/2-4, 0,
				setting->color[1], SCREEN_WIDTH/2-20, y/2-4, 0);

			if(hddFormated==1){

				sprintf(c, "HDD SIZE: %d MB", hddSize);
				x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x)-(strlen(c)*FONT_WIDTH)/2;
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

				x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x);
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

				if(setting->TV_mode == TV_mode_NTSC) 
					y += ray+15;
				else
					y += ray+20;

				itoLine(setting->color[1], SCREEN_MARGIN, y/2-4, 0,
					setting->color[1], SCREEN_WIDTH/2-20, y/2-4, 0);

				Treat = PartyInfo[browser_sel].Treatment;
				if(Treat == TREAT_SYSTEM){
					sprintf(c, "Raw SIZE: %d MB", (int)PartyInfo[browser_sel].RawSize);
					x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x)-(strlen(c)*FONT_WIDTH)/2;
					printXY(c, x, y/2, setting->color[3], TRUE);
					y += FONT_HEIGHT;
					strcpy(c, "Reserved for system");
					printXY(c, x, y/2, setting->color[3], TRUE);
					y += FONT_HEIGHT;
					pfsFree = 0;
				} else if(Treat == TREAT_NOACCESS){
					sprintf(c, "Raw SIZE: %d MB", (int)PartyInfo[browser_sel].RawSize);
					x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x)-(strlen(c)*FONT_WIDTH)/2;
					printXY(c, x, y/2, setting->color[3], TRUE);
					y += FONT_HEIGHT;
					strcpy(c, "Inaccessible");
					printXY(c, x, y/2, setting->color[3], TRUE);
					y += FONT_HEIGHT;
					pfsFree = 0;
				} else if(Treat == TREAT_HDL){ //starts clause for HDL
					//---------- Start of clause for HDL game partitions ----------
					//dlanor NB: Not properly implemented yet
					sprintf(c, "HDL SIZE: %d MB", (int)PartyInfo[browser_sel].RawSize);
					x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x)-(strlen(c)*FONT_WIDTH)/2;
					printXY(c, x, y/2, setting->color[3], TRUE);
					y += FONT_HEIGHT*2;
					pfsFree = 0;
					//---------- End of clause for HDL game partitions ----------
			  }else{ //ends clause for HDL, starts clause for normal partitions
					//---------- Start of clause for PFS partitions ----------

					sprintf(c, "PFS SIZE: %d MB", (int)PartyInfo[browser_sel].TotalSize);
					x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x)-(strlen(c)*FONT_WIDTH)/2;
					printXY(c, x, y/2, setting->color[3], TRUE);
					y += FONT_HEIGHT;
					sprintf(c, "PFS USED: %d MB", (int)PartyInfo[browser_sel].UsedSize);
					printXY(c, x, y/2, setting->color[3], TRUE);
					y += FONT_HEIGHT;
					sprintf(c, "PFS FREE: %d MB", (int)PartyInfo[browser_sel].FreeSize);
					printXY(c, x, y/2, setting->color[3], TRUE);

					pfsFree = (PartyInfo[browser_sel].FreeSize * 100) / PartyInfo[browser_sel].TotalSize;

					//---------- End of clause for normal partitions (not HDL games) ----------
				} //ends clause for normal partitions

					x = ((((SCREEN_WIDTH/2-25)-Menu_start_x)/2)+Menu_start_x);
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
