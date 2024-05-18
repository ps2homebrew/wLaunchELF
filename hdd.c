//--------------------------------------------------------------
// File name:    hdd.c
//--------------------------------------------------------------
#include "launchelf.h"

#define MAX_PARTGB 128                  // Partition MAX in GB
#define MAX_PARTMB (MAX_PARTGB * 1024)  // Partition MAX in MB

typedef struct
{
    char Name[MAX_PART_NAME + 1];
    u64 RawSize;
    u64 TotalSize;
    u64 FreeSize;
    u64 UsedSize;
    GameInfo Game;  // Intended for HDL game info, implemented through IRX module
    int Count;
    int Treatment;
} PARTYINFO;

enum {               // For Treatment codes
    TREAT_PFS = 0,   // PFS partition for normal file access
    TREAT_HDL_RAW,   // HDL game partition before reading GameInfo data
    TREAT_HDL_GAME,  // HDL game partition after reading GameInfo data
    TREAT_SYSTEM,    // System partition that must never be modified (__mbr)
    TREAT_NOACCESS   // Inaccessible partition, in fact all other partitions
};

enum {  // For menu commands
    CREATE = 0,
    REMOVE,
    RENAME,
    EXPAND,
    FORMAT,
    NUM_MENU
};

#define SECTORS_PER_MB 2048  // Divide by this to convert from sector count to MB
#define MB             1048576

static PARTYINFO PartyInfo[MAX_PARTITIONS];
static int numParty;
static u32 hddSize, hddFree, hddFreeSpace, hddUsed;
static int hddConnected, hddFormated;

static char DbgMsg[MAX_TEXT_LINE * 30];

//--------------------------------------------------------------
// radians to degrees helper functions
float sindgf(float f) { return sinf(f) * M_PI / 180.0f; }
float cosdgf(float f) { return cosf(f) * M_PI / 180.0f; }

//--------------------------------------------------------------
///*
void DebugDisp(const char *Message)
{
    int done;

    for (done = 0; done == 0;) {
        nonDialog(Message);
        drawLastMsg();
        if (readpad() && new_pad) {
            done = 1;
        }
    }
}
//*/
//------------------------------
// endfunc DebugDisp
//--------------------------------------------------------------
/*
void DebugDispStat(iox_dirent_t *p)
{
    char buf[MAX_TEXT_LINE*24]="";
    int  stp, pos, i;
    unsigned int *ip = &p->stat.private_0;
    int *cp = (int *) p->stat.ctime;
    int *ap = (int *) p->stat.atime;
    int *mp = (int *) p->stat.mtime;

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
    DebugDisp(buf);
}
//*/
//------------------------------
// endfunc DebugDispStat
//--------------------------------------------------------------
void GetHddInfo(void)
{
    iox_dirent_t infoDirEnt;
    int rv, hddFd = 0, partitionFd, i, Treat = TREAT_NOACCESS, tooManyPartitions;
    u32 size = 0, zoneFree, zoneSize;
    char tmp[MAX_PATH];
    char dbgtmp[MAX_PATH];
    //	char pfs_str[6];

    //	strcpy(pfs_str, "pfs0:");
    hddSize = 0, hddFree = 0, hddFreeSpace = 0, hddUsed = 0;
    hddConnected = 0, hddFormated = 0;
    numParty = 0;
    tooManyPartitions = 0;

    drawMsg(LNG(Reading_HDD_Information));

    if (hddCheckPresent() < 0) {
        hddConnected = 0;
        goto end;
    } else
        hddConnected = 1;

    if (hddCheckFormatted() < 0) {
        hddFormated = 0;
        goto end;
    } else
        hddFormated = 1;

    hddSize = (u32)fileXioDevctl("hdd0:", HDIOC_TOTALSECTOR, NULL, 0, NULL, 0) / SECTORS_PER_MB;

    if ((hddFd = fileXioDopen("hdd0:")) < 0)
        return;

    while (fileXioDread(hddFd, &infoDirEnt) > 0) {  // Starts main while loop
        u64 found_size = infoDirEnt.stat.size / SECTORS_PER_MB;

        // DebugDispStat(&infoDirEnt);

        if (infoDirEnt.stat.mode == APA_TYPE_FREE)
            continue;
        hddUsed += found_size;
        for (i = 0; i < numParty; i++)  // Check with previous partitions
            if (!strcmp(infoDirEnt.name, PartyInfo[i].Name))
                break;
        if (i < numParty) {                      // found another reference to an old name
            PartyInfo[i].RawSize += found_size;  // Add new segment to old size
        } else {                                 // Starts clause for finding brand new name for PartyInfo[numParty]
            if (numParty >= MAX_PARTITIONS) {
                tooManyPartitions = 1;
                break;
            }

            sprintf(dbgtmp, "%s \"%s\"", LNG(Found), infoDirEnt.name);
            drawMsg(dbgtmp);

            memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
            memcpy(PartyInfo[numParty].Name, infoDirEnt.name, MAX_PART_NAME);
            PartyInfo[numParty].Name[MAX_PART_NAME] = '\0';
            PartyInfo[numParty].RawSize = found_size;  // Store found segment size
            PartyInfo[numParty].Count = numParty;

            if (infoDirEnt.stat.mode == APA_TYPE_MBR)  // New test of partition type by 'mode'
                Treat = TREAT_SYSTEM;
            else if (infoDirEnt.stat.mode == 0x1337)
                Treat = TREAT_HDL_RAW;
            else if (infoDirEnt.stat.mode == APA_TYPE_PFS)
                Treat = TREAT_PFS;
            else
                Treat = TREAT_NOACCESS;
            PartyInfo[numParty].Treatment = Treat;

            if (Treat == TREAT_PFS) {  // Starts clause for TREAT_PFS
                sprintf(tmp, "hdd0:%s", PartyInfo[numParty].Name);
                partitionFd = open(tmp, O_RDONLY, 0);
                if (partitionFd >= 0) {
                    for (i = 0, size = 0; i < infoDirEnt.stat.private_0 + 1; i++) {
                        rv = fileXioIoctl2(partitionFd, HIOCGETSIZE, &i, 4, NULL, 0);
                        size += (u32)rv / SECTORS_PER_MB;
                    }
                    PartyInfo[numParty].TotalSize = size;
                    //			PartyInfo[numParty].RawSize = size;
                    close(partitionFd);


                    //			mountParty(tmp);
                    //			pfs_str[3] = '0'+latestMount;
                    fileXioUmount("pfs0:");
                    if (fileXioMount("pfs0:", tmp, FIO_MT_RDONLY) < 0)
                        PartyInfo[numParty].Treatment = TREAT_NOACCESS;  // non-pfs partition
                    else {
                        zoneFree = (u32)fileXioDevctl("pfs0:", PDIOC_ZONEFREE, NULL, 0, NULL, 0);
                        zoneSize = (u32)fileXioDevctl("pfs0:", PDIOC_ZONESZ, NULL, 0, NULL, 0);

                        PartyInfo[numParty].FreeSize = (u64)zoneFree * zoneSize / MB;
                        PartyInfo[numParty].UsedSize = PartyInfo[numParty].TotalSize - PartyInfo[numParty].FreeSize;
                    }
                } else {  // Ends clause for partitionFd >= 0
                    PartyInfo[numParty].Treatment = TREAT_NOACCESS;
                    PartyInfo[numParty].FreeSize = 0;
                    PartyInfo[numParty].UsedSize = 0;
                }
            }  // Ends clause for TREAT_PFS

            numParty++;
        }  // Ends clause for finding brand new name for PartyInfo[numParty]
    }      // ends main while loop
    fileXioDclose(hddFd);
    hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80;  // free space rounded to useful area
    hddFree = (hddFreeSpace * 100) / hddSize;         // free space percentage

end:
    drawMsg(tooManyPartitions ? LNG(HDD_Information_Read_Overflow) : LNG(HDD_Information_Read));

    WaitTime = Timer();
    while (Timer() < WaitTime + 1500)
        ;  // print operation result during 1.5 sec.
}
//------------------------------
// endfunc GetHddInfo
//--------------------------------------------------------------
int sizeSelector(int size)
{
    int x, y;
    int saveSize = size;
    float scrollBar;
    char c[MAX_PATH];
    int event, post_event = 0;

    int mSprite_X1 = SCREEN_WIDTH / 2 - 12 * FONT_WIDTH;   // Left edge of sprite
    int mSprite_Y1 = SCREEN_HEIGHT / 2 - 3 * FONT_HEIGHT;  // Top edge of sprite
    int mSprite_X2 = SCREEN_WIDTH / 2 + 12 * FONT_WIDTH;   // Right edge of sprite
    int mSprite_Y2 = SCREEN_HEIGHT / 2 + 3 * FONT_HEIGHT;  // Bottom edge of sprite

    event = 1;  // event = initial entry
    while (1) {
        // Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_RIGHT) {
                event |= 2;  // event |= valid pad command
                if ((size += 128) > MAX_PARTMB)
                    size = MAX_PARTMB;
            } else if (new_pad & PAD_LEFT) {
                event |= 2;  // event |= valid pad command
                if ((size -= 128) < saveSize)
                    size = saveSize;
            } else if (new_pad & PAD_R1) {
                event |= 2;  // event |= valid pad command
                if (size < 1024)
                    size = 1024;
                else {
                    if ((size += 1024) > MAX_PARTMB)
                        size = MAX_PARTMB;
                }
            } else if (new_pad & PAD_L1) {
                event |= 2;  // event |= valid pad command
                if ((size -= 1024) < saveSize)
                    size = saveSize;
            } else if (new_pad & PAD_R2) {
                event |= 2;  // event |= valid pad command
                if (size < 10240)
                    size = 10240;
                else {
                    if ((size += 10240) > MAX_PARTMB)
                        size = MAX_PARTMB;
                }
            } else if (new_pad & PAD_L2) {
                event |= 2;  // event |= valid pad command
                if ((size -= 10240) < saveSize)
                    size = saveSize;
            } else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                return -1;
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                break;
            }
        }

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            drawPopSprite(setting->color[COLOR_BACKGR],
                          mSprite_X1, mSprite_Y1,
                          mSprite_X2, mSprite_Y2);
            drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

            sprintf(c, "%d %s", size, LNG(MB));
            printXY(c, mSprite_X1 + 12 * FONT_WIDTH - (strlen(c) * FONT_WIDTH) / 2,
                    mSprite_Y1 + FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
            drawFrame(mSprite_X1 + 7 * FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT / 2,
                      mSprite_X2 - 7 * FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT * 2 + FONT_HEIGHT / 2, setting->color[COLOR_FRAME]);

            // RA NB: Next line assumes a scrollbar 19 characters wide (see below)
            sprintf(c, "128%s            %3d%s", LNG(MB), MAX_PARTGB, LNG(GB));
            printXY(c, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + FONT_HEIGHT * 3, setting->color[COLOR_TEXT], TRUE, 0);

            drawOpSprite(setting->color[COLOR_FRAME],
                         mSprite_X1 + 2 * FONT_WIDTH + FONT_WIDTH / 2, mSprite_Y1 + FONT_HEIGHT * 5 - LINE_THICKNESS + 1,
                         mSprite_X2 - 2 * FONT_WIDTH - FONT_WIDTH / 2, mSprite_Y1 + FONT_HEIGHT * 5);


            // RA NB: Next line sets scroll position on a bar 19 chars wide (see above)
            scrollBar = (size * (19 * FONT_WIDTH) / (MAX_PARTMB - 128));

            drawOpSprite(setting->color[COLOR_FRAME],
                         mSprite_X1 + 2 * FONT_WIDTH + FONT_WIDTH / 2 + (int)scrollBar - LINE_THICKNESS + 1,
                         mSprite_Y1 + FONT_HEIGHT * 5 - FONT_HEIGHT / 2,
                         mSprite_X1 + 2 * FONT_WIDTH + FONT_WIDTH / 2 + (int)scrollBar,
                         mSprite_Y1 + FONT_HEIGHT * 5 + FONT_HEIGHT / 2);

            // Tooltip section
            x = SCREEN_MARGIN;
            y = Menu_tooltip_y;
            drawSprite(setting->color[COLOR_BACKGR],
                       0, y - 1,
                       SCREEN_WIDTH, y + 16);
            if (swapKeys)
                sprintf(c, "\xFF"
                           "1:%s \xFF"
                           "0:%s \xFF"
                           "3:%s \xFF"
                           "</\xFF"
                           "::-/+128%s L1/R1:-/+1%s L2/R2:-/+10%s",
                        LNG(OK), LNG(Cancel), LNG(Back), LNG(MB), LNG(GB), LNG(GB));
            else
                sprintf(c, "\xFF"
                           "0:%s \xFF"
                           "1:%s \xFF"
                           "3:%s \xFF"
                           "</\xFF"
                           "::-/+128%s L1/R1:-/+1%s L2/R2:-/+10%s",
                        LNG(OK), LNG(Cancel), LNG(Back), LNG(MB), LNG(GB), LNG(GB));
            printXY(c, x, y, setting->color[COLOR_SELECT], TRUE, 0);
        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while
    return size;
}
//------------------------------
// endfunc sizeSelector
//--------------------------------------------------------------
int MenuParty(PARTYINFO Info)
{
    u64 color;
    char enable[NUM_MENU], tmp[64];
    int x, y, i, sel;
    int event, post_event = 0;

    int menu_len = strlen(LNG(Create)) > strlen(LNG(Remove)) ?
                       strlen(LNG(Create)) :
                       strlen(LNG(Remove));
    menu_len = strlen(LNG(Rename)) > menu_len ? strlen(LNG(Rename)) : menu_len;
    menu_len = strlen(LNG(Expand)) > menu_len ? strlen(LNG(Expand)) : menu_len;
    menu_len = strlen(LNG(Format)) > menu_len ? strlen(LNG(Format)) : menu_len;

    int menu_ch_w = menu_len + 1;                                 // Total characters in longest menu string
    int menu_ch_h = NUM_MENU;                                     // Total number of menu lines
    int mSprite_Y1 = 64;                                          // Top edge of sprite
    int mSprite_X2 = SCREEN_WIDTH - 35;                           // Right edge of sprite
    int mSprite_X1 = mSprite_X2 - (menu_ch_w + 3) * FONT_WIDTH;   // Left edge of sprite
    int mSprite_Y2 = mSprite_Y1 + (menu_ch_h + 1) * FONT_HEIGHT;  // Bottom edge of sprite

    unmountAll();     // unmount all uLE-used mountpoints
    unmountParty(0);  // unconditionally unmount primary mountpoint
    unmountParty(1);  // unconditionally unmount secondary mountpoint

    memset(enable, TRUE, NUM_MENU);

    if ((Info.Name[0] == '_') && (Info.Name[1] == '_')) {
        enable[REMOVE] = FALSE;
    }
    if (Info.Treatment == TREAT_SYSTEM) {
        enable[REMOVE] = FALSE;
        enable[RENAME] = FALSE;
        enable[EXPAND] = FALSE;
    }
    if (Info.Treatment == TREAT_HDL_RAW) {
        enable[EXPAND] = FALSE;
    }
    if (Info.Treatment == TREAT_HDL_GAME) {
        enable[EXPAND] = FALSE;
    }
    if (Info.Treatment == TREAT_NOACCESS) {
        enable[EXPAND] = FALSE;
    }
    // FIXME: Always disable the expand option, since it causes corruption
    enable[EXPAND] = FALSE;

    for (sel = 0; sel < NUM_MENU; sel++)
        if (enable[sel] == TRUE)
            break;

    event = 1;  // event = initial entry
    while (1) {
        // Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_UP && sel < NUM_MENU) {
                event |= 2;  // event |= valid pad command
                do {
                    sel--;
                    if (sel < 0)
                        sel = NUM_MENU - 1;
                } while (!enable[sel]);
            } else if (new_pad & PAD_DOWN && sel < NUM_MENU) {
                event |= 2;  // event |= valid pad command
                do {
                    sel++;
                    if (sel == NUM_MENU)
                        sel = 0;
                } while (!enable[sel]);
            } else if ((new_pad & PAD_TRIANGLE) || (!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                return -1;
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                break;
            }
        }

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            drawPopSprite(setting->color[COLOR_BACKGR],
                          mSprite_X1, mSprite_Y1,
                          mSprite_X2, mSprite_Y2);
            drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

            for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2; i < NUM_MENU; i++) {
                if (i == CREATE)
                    strcpy(tmp, LNG(Create));
                else if (i == REMOVE)
                    strcpy(tmp, LNG(Remove));
                else if (i == RENAME)
                    strcpy(tmp, LNG(Rename));
                else if (i == EXPAND)
                    strcpy(tmp, LNG(Expand));
                else if (i == FORMAT)
                    strcpy(tmp, LNG(Format));

                if (enable[i])
                    color = setting->color[COLOR_TEXT];
                else
                    color = setting->color[COLOR_FRAME];

                printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, color, TRUE, 0);
                y += FONT_HEIGHT;
            }
            if (sel < NUM_MENU)
                drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH, mSprite_Y1 + (FONT_HEIGHT / 2 + sel * FONT_HEIGHT), setting->color[COLOR_TEXT]);

            // Tooltip section
            x = SCREEN_MARGIN;
            y = Menu_tooltip_y;
            drawSprite(setting->color[COLOR_BACKGR],
                       0, y - 1,
                       SCREEN_WIDTH, y + 16);
            if (swapKeys)
                sprintf(tmp, "\xFF"
                             "1:%s \xFF"
                             "0:%s \xFF"
                             "3:%s",
                        LNG(OK), LNG(Cancel), LNG(Back));
            else
                sprintf(tmp, "\xFF"
                             "0:%s \xFF"
                             "1:%s \xFF"
                             "3:%s",
                        LNG(OK), LNG(Cancel), LNG(Back));
            printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while
    return sel;
}
//------------------------------
// endfunc MenuParty
//--------------------------------------------------------------
int CreateParty(char *party, int size)
{
    int i, num = 1, ret = 0;
    //	int  remSize = size-2048;
    char tmpName[MAX_ENTRY];
    //	t_hddFilesystem hddFs[MAX_PARTITIONS];

    drawMsg(LNG(Creating_New_Partition));

    tmpName[0] = 0;
    sprintf(tmpName, "%s", party);
    for (i = 0; i < MAX_PARTITIONS; i++) {
        if (!strcmp(PartyInfo[i].Name, tmpName)) {
            sprintf(tmpName, "%s%d", party, num);
            num++;
        }
    }
    strcpy(party, tmpName);
    /*	if(remSize <= 0)*/
    ret = hddMakeFilesystem(size, party, O_RDWR | O_CREAT);
    /*	else{
        ret = hddMakeFilesystem(2048, party, O_RDWR | O_CREAT);
        hddGetFilesystemList(hddFs, MAX_PARTITIONS);
        for(i=0; i<MAX_PARTITIONS; i++){
            if(!strcmp(hddFs[i].filename+5, party))
                ret = hddExpandFilesystem(&hddFs[i], remSize);
        }
    }*/

    if (ret > 0) {
        GetHddInfo();
        drawMsg(LNG(New_Partition_Created));
    } else {
        drawMsg(LNG(Failed_Creating_New_Partition));
    }

    WaitTime = Timer();
    while (Timer() < WaitTime + 1500)
        ;  // print operation result during 1.5 sec.

    return ret;
}
//------------------------------
// endfunc CreateParty
//--------------------------------------------------------------
int RemoveParty(PARTYINFO Info)
{
    int ret = 0;
    char tmpName[MAX_ENTRY];

    // printf("Remove Partition: %d\n", Info.Count);

    drawMsg(LNG(Removing_Current_Partition));

    sprintf(tmpName, "hdd0:%s", Info.Name);
    ret = fileXioRemove(tmpName);

    if (ret == 0) {
        hddUsed -= Info.TotalSize;
        hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80;  // free space rounded to useful area
        hddFree = (hddFreeSpace * 100) / hddSize;         // free space percentage
        numParty--;
        if (Info.Count == numParty) {
            memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
        } else {
            int i;

            for (i = Info.Count; i < numParty; i++) {
                memset(&PartyInfo[i], 0, sizeof(PARTYINFO));
                memcpy(&PartyInfo[i], &PartyInfo[i + 1], sizeof(PARTYINFO));
                PartyInfo[i].Count--;
            }
            memset(&PartyInfo[numParty], 0, sizeof(PARTYINFO));
        }
        drawMsg(LNG(Partition_Removed));
    } else {
        drawMsg(LNG(Failed_Removing_Current_Partition));
    }

    WaitTime = Timer();
    while (Timer() < WaitTime + 1500)
        ;  // print operation result during 1.5 sec.

    return ret;
}
//------------------------------
// endfunc RemoveParty
//--------------------------------------------------------------
int RenameParty(PARTYINFO Info, char *newName)
{
    int i, num = 1, ret = 0;
    char in[MAX_ENTRY], out[MAX_ENTRY], tmpName[MAX_ENTRY];

    drawMsg(LNG(Renaming_Partition));

    in[0] = 0;
    out[0] = 0;
    tmpName[0] = 0;
    sprintf(tmpName, "%s", newName);
    if (!strcmp(Info.Name, tmpName))  // Exactly the same name entered.
        goto end;
    // If other partitions exist with the same name, append a number to keep the name unique.
    for (i = 0; i < numParty; i++) {
        if (!strcmp(PartyInfo[i].Name, tmpName)) {
            sprintf(tmpName, "%s%d", newName, num);
            num++;
        }
    }
    strcpy(newName, tmpName);
    if (strlen(newName) >= MAX_PART_NAME) {
        ret = -ENAMETOOLONG;
        goto end2;  // For this case HDL renaming can't be done
    }

    sprintf(in, "hdd0:%s", Info.Name);
    sprintf(out, "hdd0:%s", newName);
    ret = fileXioRename(in, out);

    if (ret == 0) {
        strncpy(PartyInfo[Info.Count].Name, newName, MAX_PART_NAME);
        PartyInfo[Info.Count].Name[MAX_PART_NAME] = '\0';
        drawMsg(LNG(Partition_Renamed));
    } else {
    end2:
        drawMsg(LNG(Failed_Renaming_Partition));
    }

    WaitTime = Timer();
    while (Timer() < WaitTime + 1500)
        ;  // print operation result during 1.5 sec.

end:
    return ret;
}
//------------------------------
// endfunc RenameParty
//--------------------------------------------------------------
int RenameGame(PARTYINFO Info, char *newName)
{
    // printf("Rename Game: %d\n", Info.Count);
    // int fd;
    int i, num = 1, ret = 0;
    char tmpName[MAX_ENTRY];

    drawMsg(LNG(Renaming_Game));

    if (!strcmp(Info.Game.Name, newName))
        goto end1;

    tmpName[0] = 0;
    strcpy(tmpName, newName);
    for (i = 0; i < MAX_PARTITIONS; i++) {
        if (!strcmp(PartyInfo[i].Game.Name, tmpName)) {
            sprintf(tmpName, "%s%d", newName, num);
            num++;
        }
    }
    strcpy(newName, tmpName);
    if (strlen(newName) >= MAX_PART_NAME) {
        ret = -ENAMETOOLONG;
        goto end2;  // For this case HDL renaming can't be done
    }

    ret = HdlRenameGame(Info.Game.Name, newName);

    if (ret == 0) {
        strcpy(PartyInfo[Info.Count].Game.Name, newName);
        /*	if(mountParty("hdd0:HDLoader Settings")>=0){
            if((fd=genOpen("pfs0:/gamelist.log", O_RDONLY)) >= 0){
                genClose(fd);
                if(fileXioRemove("pfs0:/gamelist.log")!=0)
                    ret=0;
            }
            unmountParty(latestMount);
        }*/
    } else {
        sprintf(DbgMsg, "HdlRenameGame(\"%s\",\n  \"%s\")\n=> %d", Info.Game.Name, newName, ret);
        DebugDisp(DbgMsg);
    }


    if (ret != 0) {
        strcpy(PartyInfo[Info.Count].Game.Name, newName);
        drawMsg(LNG(Game_Renamed));
    } else {
    end2:
        drawMsg(LNG(Failed_Renaming_Game));
    }

    WaitTime = Timer();
    while (Timer() < WaitTime + 1500)
        ;  // print operation result during 1.5 sec.

end1:
    return ret;
}
//------------------------------
// endfunc RenameGame
//--------------------------------------------------------------
int ExpandParty(PARTYINFO Info, int size)
{
    int i, ret = 0;
    char tmpName[MAX_ENTRY + 5];
    t_hddFilesystem hddFs[MAX_PARTITIONS];

    drawMsg(LNG(Expanding_Current_Partition));
    // printf("Expand Partition: %d\n", Info.Count);

    sprintf(tmpName, "hdd0:%s", Info.Name);

    hddGetFilesystemList(hddFs, MAX_PARTITIONS);
    for (i = 0; i < MAX_PARTITIONS; i++) {
        if (!strcmp(hddFs[i].filename, tmpName)) {
            ret = hddExpandFilesystem(&hddFs[i], size);
        }
    }

    if (ret > 0) {
        hddUsed += size;
        hddFreeSpace = (hddSize - hddUsed) & 0x7FFFFF80;  // free space rounded to useful area
        hddFree = (hddFreeSpace * 100) / hddSize;         // free space percentage

        PartyInfo[Info.Count].TotalSize += size;
        PartyInfo[Info.Count].FreeSize += size;
        drawMsg(LNG(Partition_Expanded));
    } else {
        drawMsg(LNG(Failed_Expanding_Current_Partition));
    }

    WaitTime = Timer();
    while (Timer() < WaitTime + 1500)
        ;  // print operation result during 1.5 sec.

    return ret;
}
//------------------------------
// endfunc ExpandParty
//--------------------------------------------------------------
int FormatHdd(void)
{
    int ret = 0;

    drawMsg(LNG(Formating_HDD));

    ret = hddFormat();

    if (ret == 0) {
        drawMsg(LNG(HDD_Formated));
    } else {
        drawMsg(LNG(HDD_Format_Failed));
    }

    WaitTime = Timer();
    while (Timer() < WaitTime + 1500)
        ;  // print operation result during 1.5 sec.

    GetHddInfo();

    return ret;
}
//------------------------------
// endfunc FormatHdd
//--------------------------------------------------------------
void hddManager(void)
{
    char c[MAX_PATH];
    int Angle;
    int x, y, y0, y1, x2, y2;
    float x3, y3;
    int i, ret;
    int partySize;
    int pfsFree;
    int ray;
    u64 Color;
    char tmp[MAX_PATH];
    char tooltip[MAX_TEXT_LINE];
    int top = 0, rows;
    int event, post_event = 0;
    int browser_sel = 0, browser_nfiles;
    int Treat;

    rows = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;

    loadHddModules();
    GetHddInfo();

    event = 1;  // event = initial entry
    while (1) {

        // Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad) {
                // printf("Selected Partition: %d\n", browser_sel);
                event |= 2;  // event |= pad command
            }
            if (new_pad & PAD_UP)
                browser_sel--;
            else if (new_pad & PAD_DOWN)
                browser_sel++;
            else if (new_pad & PAD_LEFT)
                browser_sel -= rows / 2;
            else if (new_pad & PAD_RIGHT)
                browser_sel += rows / 2;
            else if ((new_pad & PAD_SELECT) || (new_pad & PAD_TRIANGLE)) {
                // Prepare for exit from HddManager
                unmountAll();     // unmount all uLE-used mountpoints
                unmountParty(0);  // unconditionally unmount primary mountpoint
                unmountParty(1);  // unconditionally unmount secondary mountpoint
                return;
            } else if (new_pad & PAD_SQUARE) {
                if (PartyInfo[browser_sel].Treatment == TREAT_HDL_RAW) {
                    loadHdlInfoModule();
                    ret = HdlGetGameInfo(PartyInfo[browser_sel].Name, &PartyInfo[browser_sel].Game);
                    if (ret == 0)
                        PartyInfo[browser_sel].Treatment = TREAT_HDL_GAME;
                    else {
                        sprintf(DbgMsg, "HdlGetGameInfo(\"%s\",bf)\n=> %d", PartyInfo[browser_sel].Name, ret);
                        DebugDisp(DbgMsg);
                    }
                } else if (PartyInfo[browser_sel].Treatment == TREAT_HDL_GAME)
                    PartyInfo[browser_sel].Treatment = TREAT_HDL_RAW;  // Press Square again to unload HDL info
            } else if (new_pad & PAD_R1) {                             // Starts clause for R1 menu
                ret = MenuParty(PartyInfo[browser_sel]);
                tmp[0] = 0;
                if (ret == CREATE) {
                    drawMsg(LNG(Enter_New_Partition_Name));
                    drawMsg(LNG(Enter_New_Partition_Name));
                    if (keyboard(tmp, MAX_PART_NAME) > 0) {
                        partySize = 128;
                        drawMsg(LNG(Select_New_Partition_Size_In_MB));
                        drawMsg(LNG(Select_New_Partition_Size_In_MB));
                        if ((ret = sizeSelector(partySize)) > 0) {
                            if (ynDialog(LNG(Create_New_Partition)) == 1) {
                                CreateParty(tmp, ret);
                                nparties = 0;  // Tell FileBrowser to refresh party list
                            }
                        }
                    }
                } else if (ret == REMOVE) {
                    if (ynDialog(LNG(Remove_Current_Partition)) == 1) {
                        RemoveParty(PartyInfo[browser_sel]);
                        nparties = 0;  // Tell FileBrowser to refresh party list
                    }
                } else if (ret == RENAME) {
                    drawMsg(LNG(Enter_New_Partition_Name));
                    drawMsg(LNG(Enter_New_Partition_Name));
                    if (PartyInfo[browser_sel].Treatment == TREAT_HDL_GAME) {  // Rename HDL Game
                        strcpy(tmp, PartyInfo[browser_sel].Game.Name);
                        if (keyboard(tmp, MAX_PART_NAME) > 0) {
                            if (ynDialog(LNG(Rename_Current_Game)) == 1)
                                RenameGame(PartyInfo[browser_sel], tmp);
                        }
                    } else {  // starts clause for normal partition RENAME
                        strcpy(tmp, PartyInfo[browser_sel].Name);
                        if (keyboard(tmp, MAX_PART_NAME) > 0) {
                            if (ynDialog(LNG(Rename_Current_Partition)) == 1) {
                                RenameParty(PartyInfo[browser_sel], tmp);
                                nparties = 0;  // Tell FileBrowser to refresh party list
                            }
                        }
                    }  // ends clause for normal partition RENAME
                } else if (ret == EXPAND) {
                    drawMsg(LNG(Select_New_Partition_Size_In_MB));
                    drawMsg(LNG(Select_New_Partition_Size_In_MB));
                    partySize = PartyInfo[browser_sel].TotalSize;
                    if ((ret = sizeSelector(partySize)) > 0) {
                        if (ynDialog(LNG(Expand_Current_Partition)) == 1) {
                            ret -= partySize;
                            ExpandParty(PartyInfo[browser_sel], ret);
                            nparties = 0;  // Tell FileBrowser to refresh party list
                        }
                    }
                } else if (ret == FORMAT) {
                    if (ynDialog(LNG(Format_HDD)) == 1) {
                        FormatHdd();
                        nparties = 0;  // Tell FileBrowser to refresh party list
                    }
                }
            }  // Ends clause for R1 menu
        }      // ends pad response section

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            clrScr(setting->color[COLOR_BACKGR]);

            browser_nfiles = numParty;
            // printf("Number Of Partition: %d\n", numParty);

            if (top > browser_nfiles - rows)
                top = browser_nfiles - rows;
            if (top < 0)
                top = 0;
            if (browser_sel >= browser_nfiles)
                browser_sel = browser_nfiles - 1;
            if (browser_sel < 0)
                browser_sel = 0;
            if (browser_sel >= top + rows)
                top = browser_sel - rows + 1;
            if (browser_sel < top)
                top = browser_sel;

            y = Menu_start_y;

            x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(LNG(HDD_STATUS)) * FONT_WIDTH) / 2;
            printXY(LNG(HDD_STATUS), x, y, setting->color[COLOR_TEXT], TRUE, 0);

            if (TV_mode != TV_mode_PAL)
                y += FONT_HEIGHT + 10;
            else
                y += FONT_HEIGHT + 11;

            drawOpSprite(setting->color[COLOR_FRAME],
                         SCREEN_MARGIN, y - 6,
                         SCREEN_WIDTH / 2 - 20, y - 4);

            if (hddConnected == 0)
                sprintf(c, "%s:  %s / %s:  %s",
                        LNG(CONNECTED), LNG(NO), LNG(FORMATED), LNG(NO));
            else if ((hddConnected == 1) && (hddFormated == 0))
                sprintf(c, "%s:  %s / %s:  %s",
                        LNG(CONNECTED), LNG(YES), LNG(FORMATED), LNG(NO));
            else if (hddFormated == 1)
                sprintf(c, "%s:  %s / %s:  %s",
                        LNG(CONNECTED), LNG(YES), LNG(FORMATED), LNG(YES));

            x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

            drawOpSprite(setting->color[COLOR_FRAME],
                         SCREEN_WIDTH / 2 - 21, Frame_start_y,
                         SCREEN_WIDTH / 2 - 19, Frame_end_y);

            if (TV_mode != TV_mode_PAL)
                y += FONT_HEIGHT + 11;
            else
                y += FONT_HEIGHT + 12;

            drawOpSprite(setting->color[COLOR_FRAME],
                         SCREEN_MARGIN, y - 6,
                         SCREEN_WIDTH / 2 - 20, y - 4);

            if (hddFormated == 1) {

                sprintf(c, "%s: %u %s", LNG(HDD_SIZE), hddSize, LNG(MB));
                x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                y += FONT_HEIGHT;
                sprintf(c, "%s: %u %s", LNG(HDD_USED), hddUsed, LNG(MB));
                printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                y += FONT_HEIGHT;
                sprintf(c, "%s: %u %s", LNG(HDD_FREE), hddFreeSpace, LNG(MB));
                printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

                if (TV_mode != TV_mode_PAL)
                    ray = 45;
                else
                    ray = 55;

                x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x);
                if (TV_mode != TV_mode_PAL)
                    y += ray + 20;
                else
                    y += ray + 25;

                for (i = 0; i < 360; i++) {
                    Angle = i - 90;
                    if (((i * 100) / 360) >= hddFree)
                        Color = setting->color[COLOR_GRAPH2];
                    else
                        Color = setting->color[COLOR_GRAPH1];
                    x3 = ray * cosdgf(Angle);
                    if (TV_mode != TV_mode_PAL)
                        y3 = (ray - 5) * sindgf(Angle);
                    else
                        y3 = (ray)*sindgf(Angle);
                    x2 = x + x3;
                    y2 = y + (y3);
                    gsKit_prim_line(gsGlobal, x, y, x2, y2, 1, Color);
                    gsKit_prim_line(gsGlobal, x, y, x2 + 1, y2, 1, Color);
                    gsKit_prim_line(gsGlobal, x, y, x2, y2 + 1, 1, Color);
                }

                sprintf(c, "%d%% %s", hddFree, LNG(FREE));
                printXY(c, x - FONT_WIDTH * 4, y - FONT_HEIGHT / 4, setting->color[COLOR_TEXT], TRUE, 0);

                if (TV_mode != TV_mode_PAL)
                    y += ray + 15;
                else
                    y += ray + 20;

                drawOpSprite(setting->color[COLOR_FRAME],
                             SCREEN_MARGIN, y - 6,
                             SCREEN_WIDTH / 2 - 20, y - 4);

                Treat = PartyInfo[browser_sel].Treatment;
                if (Treat == TREAT_SYSTEM) {
                    sprintf(c, "%s: %d %s", LNG(Raw_SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT;
                    strcpy(c, LNG(Reserved_for_system));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT;
                    pfsFree = 0;
                } else if (Treat == TREAT_NOACCESS) {
                    sprintf(c, "%s: %d %s", LNG(Raw_SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT;
                    strcpy(c, LNG(Inaccessible));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT;
                    pfsFree = 0;
                } else if (Treat == TREAT_HDL_RAW) {  // starts clause for HDL without GameInfo
                    //---------- Start of clause for HDL game partitions ----------
                    // dlanor NB: Not properly implemented yet
                    sprintf(c, "%s: %d %s", LNG(Raw_SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT * 4;
                    strcpy(c, LNG(Info_not_loaded));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    pfsFree = -1;                      // Disable lower pie chart display
                } else if (Treat == TREAT_HDL_GAME) {  // starts clause for HDL with GameInfo
                    y += FONT_HEIGHT / 4;

                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) -
                        (strlen(LNG(GAME_INFORMATION)) * FONT_WIDTH) / 2;
                    printXY(LNG(GAME_INFORMATION), x, y,
                            setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(PartyInfo[browser_sel].Game.Name) * FONT_WIDTH) / 2;
                    y += FONT_HEIGHT * 2;
                    printXY(PartyInfo[browser_sel].Game.Name, x, y,
                            setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

                    y += FONT_HEIGHT + FONT_HEIGHT / 2 + FONT_HEIGHT / 4;
                    sprintf(c, "%s: %s", LNG(STARTUP), PartyInfo[browser_sel].Game.Startup);
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT + FONT_HEIGHT / 2;
                    sprintf(c, "%s: %d %s", LNG(SIZE), (int)PartyInfo[browser_sel].RawSize, LNG(MB));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT + FONT_HEIGHT / 2;
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) -
                        (strlen(LNG(TYPE_DVD_GAME)) * FONT_WIDTH) / 2;
                    if (PartyInfo[browser_sel].Game.Is_Dvd == 1)
                        printXY(LNG(TYPE_DVD_GAME), x, y,
                                setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    else
                        printXY(LNG(TYPE_CD_GAME), x, y,
                                setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    pfsFree = -1;  // Disable lower pie chart display
                                   //---------- End of clause for HDL game partitions ----------
                } else {           // ends clause for HDL, starts clause for normal partitions
                    //---------- Start of clause for PFS partitions ----------

                    sprintf(c, "%s: %d %s", LNG(PFS_SIZE), (int)PartyInfo[browser_sel].TotalSize, LNG(MB));
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x) - (strlen(c) * FONT_WIDTH) / 2;
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT;
                    sprintf(c, "%s: %d %s", LNG(PFS_USED), (int)PartyInfo[browser_sel].UsedSize, LNG(MB));
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));
                    y += FONT_HEIGHT;
                    sprintf(c, "%s: %d %s", LNG(PFS_FREE), (int)PartyInfo[browser_sel].FreeSize, LNG(MB));
                    printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, ((SCREEN_WIDTH / 2 - 20) - SCREEN_MARGIN - 2 * FONT_WIDTH));

                    pfsFree = (PartyInfo[browser_sel].FreeSize * 100) / PartyInfo[browser_sel].TotalSize;

                    //---------- End of clause for normal partitions (not HDL games) ----------
                }  // ends clause for normal partitions

                if (pfsFree >= 0) {  // Will be negative when skipping this graph
                    x = ((((SCREEN_WIDTH / 2 - 25) - Menu_start_x) / 2) + Menu_start_x);
                    if (TV_mode != TV_mode_PAL)
                        y += ray + 20;
                    else
                        y += ray + 25;

                    for (i = 0; i < 360; i++) {
                        Angle = i - 90;
                        if (((i * 100) / 360) >= pfsFree)
                            Color = setting->color[COLOR_GRAPH2];
                        else
                            Color = setting->color[COLOR_GRAPH1];
                        x3 = ray * cosdgf(Angle);
                        if (TV_mode != TV_mode_PAL)
                            y3 = (ray - 5) * sindgf(Angle);
                        else
                            y3 = (ray)*sindgf(Angle);
                        x2 = x + x3;
                        y2 = y + (y3);
                        gsKit_prim_line(gsGlobal, x, y, x2, y2, 1, Color);
                        gsKit_prim_line(gsGlobal, x, y, x2 + 1, y2, 1, Color);
                        gsKit_prim_line(gsGlobal, x, y, x2, y2 + 1, 1, Color);
                    }

                    sprintf(c, "%d%% %s", pfsFree, LNG(FREE));
                    printXY(c, x - FONT_WIDTH * 4, y - FONT_HEIGHT / 2, setting->color[COLOR_TEXT], TRUE, 0);
                }

                rows = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;

                x = SCREEN_WIDTH / 2 - FONT_WIDTH;
                y = Menu_start_y;

                for (i = 0; i < rows; i++) {
                    if (top + i >= browser_nfiles)
                        break;
                    if (top + i == browser_sel)
                        Color = setting->color[COLOR_SELECT];  // Highlight cursor line
                    else
                        Color = setting->color[COLOR_TEXT];

                    strcpy(tmp, PartyInfo[top + i].Name);
                    printXY(tmp, x + 4, y, Color, TRUE, ((SCREEN_WIDTH - SCREEN_MARGIN) - (SCREEN_WIDTH / 2 - FONT_WIDTH)));
                    y += FONT_HEIGHT;
                }                             // ends for, so all browser rows were fixed above
                if (browser_nfiles > rows) {  // if more files than available rows, use scrollbar
                    drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
                              SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[COLOR_FRAME]);
                    y0 = (Menu_end_y - Menu_start_y + 8) * ((double)top / browser_nfiles);
                    y1 = (Menu_end_y - Menu_start_y + 8) * ((double)(top + rows) / browser_nfiles);
                    drawOpSprite(setting->color[COLOR_FRAME],
                                 SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, (y0 + Menu_start_y - 4),
                                 SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, (y1 + Menu_start_y - 4));
                }  // ends clause for scrollbar
            }      // ends hdd formated
            // Tooltip section
            sprintf(tooltip, "R1:%s  \xFF"
                             "3:%s",
                    LNG(MENU), LNG(Exit));
            if (PartyInfo[browser_sel].Treatment == TREAT_HDL_RAW) {
                sprintf(tmp, " \xFF"
                             "2:%s",
                        LNG(Load_HDL_Game_Info));
                strcat(tooltip, tmp);
            }
            if (PartyInfo[browser_sel].Treatment == TREAT_HDL_GAME) {
                sprintf(tmp, " \xFF"
                             "2:%s",
                        LNG(Unload_HDL_Game_Info));
                strcat(tooltip, tmp);
            }
            setScrTmp(LNG(PS2_HDD_MANAGER), tooltip);

        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while

    return;
}
//------------------------------
// endfunc hddManager
//--------------------------------------------------------------
