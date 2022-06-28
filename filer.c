//--------------------------------------------------------------
// File name:   filer.c
//--------------------------------------------------------------
#include "launchelf.h"

typedef struct
{
    unsigned char unknown;
    unsigned char sec;    // date/time (second)
    unsigned char min;    // date/time (minute)
    unsigned char hour;   // date/time (hour)
    unsigned char day;    // date/time (day)
    unsigned char month;  // date/time (month)
    unsigned short year;  // date/time (year)
} PS2TIME __attribute__((aligned(2)));

#define MC_SFI 0xFEED  // flag value used for mcSetFileInfo at MC file restoration

#define MC_ATTR_norm_folder 0x8427  // Normal folder on PS2 MC
#define MC_ATTR_prot_folder 0x842F  // Protected folder on PS2 MC
#define MC_ATTR_PS1_folder  0x9027  // PS1 save folder on PS2 MC
#define MC_ATTR_norm_file   0x8497  // file (PS2/PS1) on PS2 MC
#define MC_ATTR_PS1_file    0x9417  // PS1 save file on PS1 MC

#define IOCTL_RENAME 0xFEEDC0DE  // Ioctl request code for Rename function

enum {
    COPY,
    CUT,
    PASTE,
    MCPASTE,
    PSUPASTE,
    DELETE,
    RENAME,
    NEWDIR,
    NEWICON,
    MOUNTVMC0,
    MOUNTVMC1,
    GETSIZE,
    NUM_MENU
} R1_menu_enum;

#define PM_NORMAL      0   // PasteMode value for normal copies
#define PM_MC_BACKUP   1   // PasteMode value for gamesave backup from MC
#define PM_MC_RESTORE  2   // PasteMode value for gamesave restore to MC
#define PM_PSU_BACKUP  3   // PasteMode value for gamesave backup from MC to PSU
#define PM_PSU_RESTORE 4   // PasteMode value for gamesave restore to MC from PSU
#define PM_RENAME      5   // PasteMode value for normal copies with new names
#define MAX_RECURSE    16  // Maximum folder recursion for MC Backup/Restore

int PasteProgress_f = 0;   // Flags progress report having been made in Pasting
int PasteMode;             // Top-level PasteMode flag
int PM_flag[MAX_RECURSE];  // PasteMode flag for each 'copy' recursion level
int PM_file[MAX_RECURSE];  // PasteMode attribute file descriptors

char mountedParty[MOUNT_LIMIT][MAX_NAME];
int latestMount = -1;
char mountedDVRPParty[MOUNT_LIMIT][MAX_NAME];
int latestDVRPMount = -1;
int vmcMounted[2] = {0, 0};                          // flags true for mounted VMC false for unmounted
int vmc_PartyIndex[2] = {-1, -1};                    // PFS index for each VMC, unless -1
int Party_vmcIndex[MOUNT_LIMIT] = {-1, -1, -1, -1};  // VMC for each PFS, unless -1
unsigned char *elisaFnt = NULL;
int elisa_failed = FALSE;  // Set at failure to load font, cleared at browser entry
u64 freeSpace;
int mcfreeSpace;
int mctype_PSx;  // dlanor: Needed for proper scaling of mcfreespace
int vfreeSpace;  // flags validity of freespace value
int browser_cut;
int nclipFiles, nmarks, nparties, ndvrpparties;
int file_show = 1;  // dlanor: 0==name_only, 1==name+size+time, 2==title+size+time
int file_sort = 1;  // dlanor: 0==none, 1==name, 2==title, 3==mtime
int size_valid = 0;
int time_valid = 0;
char parties[MAX_PARTITIONS][MAX_PART_NAME + 1];
char clipPath[MAX_PATH], LastDir[MAX_NAME], marks[MAX_ENTRY];
FILEINFO clipFiles[MAX_ENTRY];
int fileMode = FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH;

char cnfmode_extU[CNFMODE_CNT][4] = {
    "*",    // cnfmode FALSE
    "ELF",  // cnfmode TRUE
    "IRX",  // cnfmode USBD_IRX_CNF
    "JPG",  // cnfmode SKIN_CNF
    "JPG",  // cnfmode GUI_SKIN_CNF
    "IRX",  // cnfmode USBKBD_IRX_CNF
    "KBD",  // cnfmode KBDMAP_FILE_CNF
    "CNF",  // cnfmode CNF_PATH_CNF
    "*",    // cnfmode TEXT_CNF
    "",     // cnfmode DIR_CNF
    "JPG",  // cnfmode JPG_CNF
    "IRX",  // cnfmode USBMASS_IRX_CNF
    "LNG",  // cnfmode LANG_CNF
    "FNT",  // cnfmode FONT_CNF
    "*"     // cnfmode SAVE_CNF
};

char cnfmode_extL[CNFMODE_CNT][4] = {
    "*",    // cnfmode FALSE
    "elf",  // cnfmode TRUE
    "irx",  // cnfmode USBD_IRX_CNF
    "jpg",  // cnfmode SKIN_CNF
    "jpg",  // cnfmode GUI_SKIN_CNF
    "irx",  // cnfmode USBKBD_IRX_CNF
    "kbd",  // cnfmode KBDMAP_FILE_CNF
    "cnf",  // cnfmode CNF_PATH_CNF
    "*",    // cnfmode TEXT_CNF
    "",     // cnfmode DIR_CNF
    "jpg",  // cnfmode JPG_CNF
    "irx",  // cnfmode USBMASS_IRX_CNF
    "lng",  // cnfmode LANG_CNF
    "fnt",  // cnfmode FONT_CNF
    "*"     // cnfmode SAVE_CNF
};

int host_ready = 0;
int host_error = 0;
int host_elflist = 0;
int host_use_Bsl = 1;  // By default assume that host paths use backslash

unsigned long written_size;  // Used for pasting progress report
u64 PasteTime;               // Used for pasting progress report

typedef struct
{
    u8 unused;
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} ps2time;

typedef struct
{                       // Offs:  Example content
    ps2time cTime;      // 0x00:  8 bytes creation timestamp (struct above)
    ps2time mTime;      // 0x08:  8 bytes modification timestamp (struct above)
    u32 size;           // 0x10:  file size
    u16 attr;           // 0x14:  0x8427  (=normal folder, 8497 for normal file)
    u16 unknown_1_u16;  // 0x16:  2 zero bytes
    u64 unknown_2_u64;  // 0x18:  8 zero bytes
    u8 name[32];        // 0x20:  32 name bytes, padded with zeroes
} mcT_header __attribute__((aligned(64)));

typedef struct
{                                   // Offs:  Example content
    u16 attr;                       // 0x00:  0x8427  (=normal folder, 8497 for normal file)
    u16 unknown_1_u16;              // 0x02:  2 zero bytes
    u32 size;                       // 0x04:  header_count-1, file size, 0 for pseudo
    ps2time cTime;                  // 0x08:  8 bytes creation timestamp (struct above)
    u64 EMS_used_u64;               // 0x10:  8 zero bytes (but used by EMS)
    ps2time mTime;                  // 0x18:  8 bytes modification timestamp (struct above)
    u64 unknown_2_u64;              // 0x20:  8 bytes from mcTable
    u8 unknown_3_24_bytes[24];      // 0x28:  24 zero bytes
    u8 name[32];                    // 0x40:  32 name bytes, padded with zeroes
    u8 unknown_4_416_bytes[0x1A0];  // 0x60:  zero byte padding to reach 0x200 size
} psu_header;                       // 0x200: End of psu_header struct

int PSU_content;  // Used to count PSU content headers for the main header

// USB_mass definitions for multiple drive usage
char USB_mass_ix[10] = {'0', 0, 0, 0, 0, 0, 0, 0, 0, 0};
int USB_mass_max_drives = USB_MASS_MAX_DRIVES;
u64 USB_mass_scan_time = 0;
int USB_mass_scanned = 0;  // 0==Not_found_OR_No_Multi 1==found_Multi_mass_once
int USB_mass_loaded = 0;   // 0==none, 1==internal, 2==external


// char debugs[4096]; //For debug display strings. Comment it out when unused
//--------------------------------------------------------------
// executable code
//--------------------------------------------------------------
void clear_mcTable(sceMcTblGetDir *mcT)
{
    memset((void *)mcT, 0, sizeof(sceMcTblGetDir));
}
//--------------------------------------------------------------
void clear_psu_header(psu_header *psu)
{
    memset((void *)psu, 0, sizeof(psu_header));
}
//--------------------------------------------------------------
void pad_psu_header(psu_header *psu)
{
    memset((void *)psu, 0xFF, sizeof(psu_header));
}
//--------------------------------------------------------------
// getHddParty below takes as input the string path and the struct file
// and uses these to calculate the output strings party and dir. If the
// file struct is not passed as NULL, then its file->name entry will be
// added to the internal copy of the path string (which remains unchanged),
// and if that file struct entry is for a folder, then a slash is also added.
// The modified path is then used to calculate the output strings as follows.
//-----
// party = the full path string with "hdd0:" and partition spec, but without
// the slash character between them, used in user specified full paths. So
// the first slash in that string will come after the partition name.
//-----
// dir = the pfs path string, starting like "pfs0:" (but may use different
// pfs index), and this is then followed by the path within that partition.
// Note that despite the name 'dir', this is also used for files.
//-----
// NB: From the first slash character those two strings are identical when
// both are used, but either pointer may be set to NULL in the function call,
// as an indication that the caller isn't interested in that part.
//--------------------------------------------------------------
int getHddParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
    char fullpath[MAX_PATH], *p;

    if (strncmp(path, "hdd", 3))
        return -1;

    strcpy(fullpath, path);
    if (file != NULL) {
        strcat(fullpath, file->name);
        if (file->stats.AttrFile & sceMcFileAttrSubdir)
            strcat(fullpath, "/");
    }
    if ((p = strchr(&fullpath[6], '/')) == NULL)
        return -1;
    if (dir != NULL)
        sprintf(dir, "pfs0:%s", p);
    *p = 0;
    if (party != NULL)
        sprintf(party, "hdd0:%s", &fullpath[6]);

    return 0;
}
//--------------------------------------------------------------
int mountParty(const char *party)
{
    int i, j;
    char pfs_str[6];

    for (i = 0; i < MOUNT_LIMIT; i++) {  // Here we check already mounted PFS indexes
        if (!strcmp(party, mountedParty[i]))
            goto return_i;
    }

    for (i = 0, j = -1; i < MOUNT_LIMIT; i++) {  // Here we search for a free PFS index
        if (mountedParty[i][0] == 0) {
            j = i;
            break;
        }
    }

    if (j == -1) {  // Here we search for a suitable PFS index to unmount
        for (i = 0; i < MOUNT_LIMIT; i++) {
            if ((i != latestMount) && (Party_vmcIndex[i] < 0)) {
                j = i;
                break;
            }
        }
        unmountParty(j);
    }
    // Here j is the index of a free PFS mountpoint
    // But 'free' only means that the main uLE program isn't using it
    // If the ftp server is running, that may have used the mountpoints

    // RA NB: The old code to reclaim FTP partitions was seriously bugged...

    i = j;
    strcpy(pfs_str, "pfs0:");

    pfs_str[3] = '0' + i;
    if (fileXioMount(pfs_str, party, FIO_MT_RDWR) < 0) {          // if FTP stole it
        for (i = 0; i < MOUNT_LIMIT; i++) {                       // for loop to kill FTP partition mountpoints
            if ((i != latestMount) && (Party_vmcIndex[i] < 0)) {  // if unneeded by uLE
                unmountParty(i);                                  // unmount partition mountpoint
                pfs_str[3] = '0' + i;                             // prepare to reuse that mountpoint
                if (fileXioMount(pfs_str, party, FIO_MT_RDWR) >= 0)
                    break;  // break from the loop on successful mount
            }               // ends if unneeded by uLE
        }                   // ends for loop to kill FTP partition mountpoints
        // Here i indicates what happened above with the following meanings:
        // 0..4==Success after trying i mountpoints,  5==Failure
        if (i >= MOUNT_LIMIT)
            return -1;
    }  // ends if clause for mountpoints stolen by FTP
    if (i < MOUNT_LIMIT) {
        strcpy(mountedParty[i], party);
    }
return_i:
    latestMount = i;
    return i;
}
//--------------------------------------------------------------
void unmountParty(int party_ix)
{
    char pfs_str[6];

    strcpy(pfs_str, "pfs0:");
    pfs_str[3] += party_ix;
    if (fileXioUmount(pfs_str) < 0)
        return;  // leave variables unchanged if unmount failed (remember true state)
    if (party_ix < MOUNT_LIMIT) {
        mountedParty[party_ix][0] = 0;
    }
    if (latestMount == party_ix)
        latestMount = -1;
}
//--------------------------------------------------------------
// The above modified for the DVRP.
//--------------------------------------------------------------
int getHddDVRPParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
    char fullpath[MAX_PATH], *p;

    if (strncmp(path, "dvr_hdd", 7))
        return -1;

    strcpy(fullpath, path);
    if (file != NULL) {
        strcat(fullpath, file->name);
        if (file->stats.AttrFile & sceMcFileAttrSubdir)
            strcat(fullpath, "/");
    }
    if ((p = strchr(&fullpath[10], '/')) == NULL)
        return -1;
    if (dir != NULL)
        sprintf(dir, "dvr_pfs0:%s", p);
    *p = 0;
    if (party != NULL)
        sprintf(party, "dvr_hdd0:%s", &fullpath[10]);

    return 0;
}
//--------------------------------------------------------------
int mountDVRPParty(const char *party)
{
    int i;

    for (i = 0; i < MOUNT_LIMIT; i++) {  // Here we check already mounted PFS indexes
        if (!strcmp(party, mountedDVRPParty[i]))
            goto return_i;
    }

    if (strcmp(party, "dvr_hdd0:__xdata") == 0) {
        i = 1;
    } else if (strcmp(party, "dvr_hdd0:__xcontents") == 0) {
        i = 0;
    } else {
        return -1;
    }
    strcpy(mountedDVRPParty[i], party);
return_i:
    latestDVRPMount = i;
    return i;
}
//--------------------------------------------------------------
void unmountDVRPParty(int party_ix)
{
    if (party_ix < MOUNT_LIMIT) {
        mountedDVRPParty[party_ix][0] = 0;
    }
    if (latestDVRPMount == party_ix)
        latestDVRPMount = -1;
}
//--------------------------------------------------------------
// unmountAll can unmount all mountpoints from 0 to MOUNT_LIMIT,
// but unlike the individual unmountParty, it will only do so
// for mountpoints indicated as used by the matching string in
// the string array 'mountedParty'.
// From v4.23 this routine is also used to unmount VMC devices
//------------------------------
void unmountAll(void)
{
    char pfs_str[6];
    char dvr_pfs_str[10];
    char vmc_str[6];
    int i;

    strcpy(vmc_str, "vmc0:");
    for (i = 0; i < 2; i++) {
        if (vmcMounted[i]) {
            vmc_str[3] = '0' + i;
            fileXioUmount(vmc_str);
            vmcMounted[i] = 0;
            vmc_PartyIndex[i] = -1;
        }
    }

    strcpy(pfs_str, "pfs0:");
    for (i = 0; i < MOUNT_LIMIT; i++) {
        Party_vmcIndex[i] = -1;
        if (mountedParty[i][0] != 0) {
            pfs_str[3] = '0' + i;
            fileXioUmount(pfs_str);
            mountedParty[i][0] = 0;
        }
    }
    latestMount = -1;

    strcpy(dvr_pfs_str, "dvr_pfs0:");
    for (i = 0; i < MOUNT_LIMIT; i++) {
        if (mountedDVRPParty[i][0] != 0) {
            dvr_pfs_str[7] = '0' + i;
            fileXioUmount(dvr_pfs_str);
            mountedDVRPParty[i][0] = 0;
        }
    }
    latestDVRPMount = -1;
}  // ends unmountAll
//--------------------------------------------------------------
int ynDialog(const char *message)
{
    char msg[512];
    int dh, dw, dx, dy;
    int sel = 0, a = 6, b = 4, c = 2, n, tw;
    int i, x, len, ret;
    int event, post_event = 0;

    strcpy(msg, message);

    for (i = 0, n = 1; msg[i] != 0; i++) {  // start with one string at pos zero
        if (msg[i] == '\n') {               // line separator at current pos ?
            msg[i] = 0;                     // split old line to separate string
            n++;                            // increment string count
        }
    }                                                 // loop back for next character pos
    for (i = len = tw = 0; i < n; i++) {              // start with string 0, assume 0 length & width
        ret = printXY(&msg[len], 0, 0, 0, FALSE, 0);  // get width of current string
        if (ret > tw)
            tw = ret;                  // tw = largest text width of strings so far
        len += strlen(&msg[len]) + 1;  // len = pos of next string start
    }
    if (tw < 96)
        tw = 96;

    dh = FONT_HEIGHT * (n + 1) + 2 * 2 + a + b + c;
    dw = 2 * 2 + a * 2 + tw;
    dx = (SCREEN_WIDTH - dw) / 2;
    dy = (SCREEN_HEIGHT - dh) / 2;
    //	printf("tw=%d\ndh=%d\ndw=%d\ndx=%d\ndy=%d\n", tw,dh,dw,dx,dy);

    event = 1;  // event = initial entry
    while (1) {
        // Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_LEFT) {
                event |= 2;  // event |= valid pad command
                sel = 0;
            } else if (new_pad & PAD_RIGHT) {
                event |= 2;  // event |= valid pad command
                sel = 1;
            } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                ret = -1;
                break;
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                if (sel == 0)
                    ret = 1;
                else
                    ret = -1;
                break;
            }
        }

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            drawPopSprite(setting->color[COLOR_BACKGR],
                          dx, dy,
                          dx + dw, (dy + dh));
            drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
            for (i = len = 0; i < n; i++) {
                printXY(&msg[len], dx + 2 + a, (dy + a + 2 + i * 16), setting->color[COLOR_TEXT], TRUE, 0);
                len += strlen(&msg[len]) + 1;
            }

            // Cursor positioning section
            x = (tw - 96) / 4;
            printXY(LNG(OK), dx + a + x + FONT_WIDTH,
                    (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
            printXY(LNG(CANCEL), dx + dw - x - (strlen(LNG(CANCEL)) + 1) * FONT_WIDTH,
                    (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
            if (sel == 0)
                drawChar(LEFT_CUR, dx + a + x, (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT]);
            else
                drawChar(LEFT_CUR, dx + dw - x - (strlen(LNG(CANCEL)) + 2) * FONT_WIDTH - 1,
                         (dy + a + b + 2 + n * FONT_HEIGHT), setting->color[COLOR_TEXT]);
        }  // ends if(event||post_event)
        drawLastMsg();
        post_event = event;
        event = 0;
    }  // ends while
    drawSprite(setting->color[COLOR_BACKGR], dx, dy, dx + dw + 1, (dy + dh) + 1);
    drawScr();
    drawSprite(setting->color[COLOR_BACKGR], dx, dy, dx + dw + 1, (dy + dh) + 1);
    drawScr();
    return ret;
}
//------------------------------
// endfunc ynDialog
//--------------------------------------------------------------
void nonDialog(char *message)
{
    char msg[80 * 30];          // More than this can't be shown on screen, even in PAL
    static int dh, dw, dx, dy;  // These are static, to allow cleanup
    int a = 6, b = 4, c = 2, n, tw;
    int i, len, ret;

    if (message == NULL) {  // NULL message means cleanup for last nonDialog
        drawSprite(setting->color[COLOR_BACKGR],
                   dx, dy,
                   dx + dw, (dy + dh));
        return;
    }

    strcpy(msg, message);

    for (i = 0, n = 1; msg[i] != 0; i++) {  // start with one string at pos zero
        if (msg[i] == '\n') {               // line separator at current pos ?
            msg[i] = 0;                     // split old line to separate string
            n++;                            // increment string count
        }
    }                                                 // loop back for next character pos
    for (i = len = tw = 0; i < n; i++) {              // start with string 0, assume 0 length & width
        ret = printXY(&msg[len], 0, 0, 0, FALSE, 0);  // get width of current string
        if (ret > tw)
            tw = ret;                  // tw = largest text width of strings so far
        len += strlen(&msg[len]) + 1;  // len = pos of next string start
    }
    if (tw < 96)
        tw = 96;

    dh = 16 * n + 2 * 2 + a + b + c;
    dw = 2 * 2 + a * 2 + tw;
    dx = (SCREEN_WIDTH - dw) / 2;
    dy = (SCREEN_HEIGHT - dh) / 2;
    // printf("tw=%d\ndh=%d\ndw=%d\ndx=%d\ndy=%d\n", tw,dh,dw,dx,dy);

    drawPopSprite(setting->color[COLOR_BACKGR],
                  dx, dy,
                  dx + dw, (dy + dh));
    drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
    for (i = len = 0; i < n; i++) {
        printXY(&msg[len], dx + 2 + a, (dy + a + 2 + i * FONT_HEIGHT), setting->color[COLOR_TEXT], TRUE, 0);
        len += strlen(&msg[len]) + 1;
    }
}
//------------------------------
// endfunc nonDialog
//--------------------------------------------------------------
// cmpFile below returns negative if the 'a' entry is 'lower'
// than the 'b' entry, normally meaning that 'a' should be in
// a higher/earlier screen position than 'b'. Such negative
// return value causes the calling sort routine to adjust the
// entry order, which does not occur for other return values.
//--------------------------------------------------------------
int cmpFile(FILEINFO *a, FILEINFO *b)  // Used for directory sort
{
    char ca, cb;
    int i, n, ret, aElf = FALSE, bElf = FALSE, t = (file_sort == 2);

    if (file_sort == 0)
        return 0;  // return 0 for unsorted mode

    if ((a->stats.AttrFile & MC_ATTR_OBJECT) == (b->stats.AttrFile & MC_ATTR_OBJECT)) {
        if (a->stats.AttrFile & sceMcFileAttrFile) {
            if (genCmpFileExt(a->name, "ELF"))
                aElf = TRUE;
            if (genCmpFileExt(b->name, "ELF"))
                bElf = TRUE;
            if (aElf && !bElf)
                return -1;
            else if (!aElf && bElf)
                return 1;
        }
        if (file_sort == 3) {      // Sort by timestamp
            t = (file_show == 2);  // Set secondary sort criterion
            if (time_valid) {
                u64 time_a = *(u64 *)&a->stats._Modify;
                u64 time_b = *(u64 *)&b->stats._Modify;
                if (time_a > time_b)
                    return -1;  // NB: reversed comparison for falling order
                if (time_a < time_b)
                    return 1;
            }
        }
        if (t) {
            if (a->title[0] != 0 && b->title[0] == 0)
                return -1;
            else if (a->title[0] == 0 && b->title[0] != 0)
                return 1;
            else if (a->title[0] == 0 && b->title[0] == 0)
                t = FALSE;
        }
        if (t)
            n = strlen((const char *)a->title);
        else
            n = strlen(a->name);
        for (i = 0; i <= n; i++) {
            if (t) {
                ca = a->title[i];
                cb = b->title[i];
            } else {
                ca = a->name[i];
                cb = b->name[i];
                if (ca >= 'a' && ca <= 'z')
                    ca -= 0x20;
                if (cb >= 'a' && cb <= 'z')
                    cb -= 0x20;
            }
            ret = ca - cb;
            if (ret != 0)
                return ret;
        }
        return 0;
    }

    if (a->stats.AttrFile & sceMcFileAttrSubdir)
        return -1;
    else
        return 1;
}
//--------------------------------------------------------------
void sort(FILEINFO *a, int left, int right)
{
    FILEINFO tmp, pivot;
    int i, p;

    if (left < right) {
        pivot = a[left];
        p = left;
        for (i = left + 1; i <= right; i++) {
            if (cmpFile(&a[i], &pivot) < 0) {
                p = p + 1;
                tmp = a[p];
                a[p] = a[i];
                a[i] = tmp;
            }
        }
        a[left] = a[p];
        a[p] = pivot;
        sort(a, left, p - 1);
        sort(a, p + 1, right);
    }
}
//--------------------------------------------------------------
int readMC(const char *path, FILEINFO *info, int max)
{
    static sceMcTblGetDir mcDir[MAX_ENTRY] __attribute__((aligned(64)));
    char dir[MAX_PATH];
    int i, j, ret;

    mcSync(0, NULL, NULL);

    mcGetInfo(path[2] - '0', 0, &mctype_PSx, NULL, NULL);
    mcSync(0, NULL, &ret);
    if (mctype_PSx == 2)  // PS2 MC ?
        time_valid = 1;
    size_valid = 1;

    strcpy(dir, &path[4]);
    strcat(dir, "*");
    mcGetDir(path[2] - '0', 0, dir, 0, MAX_ENTRY - 2, mcDir);
    mcSync(0, NULL, &ret);

    for (i = j = 0; i < ret; i++) {
        if (mcDir[i].AttrFile & sceMcFileAttrSubdir &&
            (!strcmp((char *)mcDir[i].EntryName, ".") || !strcmp((char *)mcDir[i].EntryName, "..")))
            continue;  // Skip pseudopaths "." and ".."
        strcpy(info[j].name, (char *)mcDir[i].EntryName);
        info[j].stats = mcDir[i];
        j++;
    }

    return j;
}
//------------------------------
// endfunc readMC
//--------------------------------------------------------------
int readCD(const char *path, FILEINFO *info, int max)
{
    iox_dirent_t record;
    int n = 0, dd = -1;
    u64 wait_start;

    if (sceCdGetDiskType() <= SCECdUNKNOWN) {
        wait_start = Timer();
        while ((Timer() < wait_start + 500) && !uLE_cdDiscValid()) {
            if (cdmode == SCECdNODISC)
                return 0;
        }
        if (cdmode == SCECdNODISC)
            return 0;
        if ((cdmode < SCECdPSCD) || (cdmode > SCECdPS2DVD)) {
            uLE_cdStop();
            return 0;
        }
    }

    if ((dd = fileXioDopen(path)) < 0)
        goto exit;  // exit if error opening directory
    while (fileXioDread(dd, &record) > 0) {
        if ((FIO_S_ISDIR(record.stat.mode)) && (!strcmp(record.name, ".") || !strcmp(record.name, "..")))
            continue;  // Skip entry if pseudo-folder "." or ".."

        strcpy(info[n].name, record.name);
        clear_mcTable(&info[n].stats);
        if (FIO_S_ISDIR(record.stat.mode)) {
            info[n].stats.AttrFile = MC_ATTR_norm_folder;
        } else if (FIO_S_ISREG(record.stat.mode)) {
            info[n].stats.AttrFile = MC_ATTR_norm_file;
            info[n].stats.FileSizeByte = record.stat.size;
            info[n].stats.Reserve2 = 0;
        } else
            continue;  // Skip entry which is neither a file nor a folder
        memcpy((char *)info[n].stats.EntryName, info[n].name, 32);
        info[n].stats.EntryName[sizeof(info[n].stats.EntryName) - 1] = 0;
        memcpy((void *)&info[n].stats._Create, record.stat.ctime, 8);
        memcpy((void *)&info[n].stats._Modify, record.stat.mtime, 8);
        n++;
        if (n == max)
            break;
    }  // ends while
    size_valid = 1;

exit:
    if (dd >= 0)
        fileXioDclose(dd);  // Close directory if opened above
    return n;
}
//------------------------------
// endfunc readCD
//--------------------------------------------------------------
void setPartyList(void)
{
    iox_dirent_t dirEnt;
    int hddFd;

    nparties = 0;

    if ((hddFd = fileXioDopen("hdd0:")) < 0)
        return;
    while (fileXioDread(hddFd, &dirEnt) > 0) {
        if (nparties >= MAX_PARTITIONS)
            break;
        if ((dirEnt.stat.attr != ATTR_MAIN_PARTITION) || (dirEnt.stat.mode != FS_TYPE_PFS))
            continue;

        // Patch this to see if new CB versions use valid PFS format
        // NB: All CodeBreaker versions up to v9.3 use invalid formats
        /*	if(!strncmp(dirEnt.name, "PP.",3)){
            int len = strlen(dirEnt.name);
            if(!strcmp(dirEnt.name+len-4, ".PCB"))
                continue;
        }

        if(!strncmp(dirEnt.name, "__", 2) &&
            strcmp(dirEnt.name, "__boot") &&
            strcmp(dirEnt.name, "__net") &&
            strcmp(dirEnt.name, "__system") &&
            strcmp(dirEnt.name, "__sysconf") &&
            strcmp(dirEnt.name, "__contents") &&   // this is where PSBBN used to store it's downloaded contents. Adding it is useful.
            strcmp(dirEnt.name, "__common"))
            continue;
    */
        memcpy(parties[nparties], dirEnt.name, MAX_PART_NAME);
        parties[nparties++][MAX_PART_NAME] = '\0';
    }
    fileXioDclose(hddFd);
}
//--------------------------------------------------------------
void setDVRPPartyList(void)
{
    iox_dirent_t dirEnt;
    int hddFd;

    ndvrpparties = 0;

    if ((hddFd = fileXioDopen("dvr_hdd0:")) < 0)
        return;
    while (fileXioDread(hddFd, &dirEnt) > 0) {
        if (ndvrpparties >= MAX_PARTITIONS)
            break;
        if ((dirEnt.stat.attr != ATTR_MAIN_PARTITION) || (dirEnt.stat.mode != FS_TYPE_PFS))
            continue;

        // Patch this to see if new CB versions use valid PFS format
        // NB: All CodeBreaker versions up to v9.3 use invalid formats
        /*	if(!strncmp(dirEnt.name, "PP.",3)){
            int len = strlen(dirEnt.name);
            if(!strcmp(dirEnt.name+len-4, ".PCB"))
                continue;
        }

        if(!strncmp(dirEnt.name, "__", 2) &&
            strcmp(dirEnt.name, "__boot") &&
            strcmp(dirEnt.name, "__net") &&
            strcmp(dirEnt.name, "__system") &&
            strcmp(dirEnt.name, "__sysconf") &&
            strcmp(dirEnt.name, "__contents") &&   // this is where PSBBN used to store it's downloaded contents. Adding it is useful.
            strcmp(dirEnt.name, "__common"))
            continue;
    */
        memcpy(parties[ndvrpparties], dirEnt.name, MAX_PART_NAME);
        parties[ndvrpparties++][MAX_PART_NAME] = '\0';
    }
    fileXioDclose(hddFd);
}
//--------------------------------------------------------------
// The following group of file handling functions are used to allow
// the main program to access files without having to deal with the
// difference between device-specific needs directly in each call.
// Even so, special paths are assumed to be already prepared so
// as to be accepted by fileXio calls (so HDD file access use "pfs")
// Generic functions for this purpose that have been added so far are:
// genInit(void), genLimObjName(uLE_path, reserve),
// genFixPath(uLE_path, gen_path),
// genOpen(path, mode), genClose(fd), genDopen(path), genDclose(fd),
// genLseek(fd,where,how), genRead(fd,buf,size), genWrite(fd,buf,size)
// genRemove(path), genRmdir(path)
//--------------------------------------------------------------
void genLimObjName(char *uLE_path, int reserve)
{
    char *p, *q, *r;
    int limit = 256;                                            // enforce a generic limit of 256 characters
    int folder_flag = (uLE_path[strlen(uLE_path) - 1] == '/');  // flag folder object
    int overflow;

    if (!strncmp(uLE_path, "mc", 2) || !strncmp(uLE_path, "vmc", 3))
        limit = 32;  // enforce MC limit of 32 characters

    if (folder_flag)                         // if path ends with path separator
        uLE_path[strlen(uLE_path) - 1] = 0;  //  remove final path separator (temporarily)

    p = uLE_path;                         // initially assume a pure object name (quite insanely :))
    if ((q = strchr(p, ':')) != NULL)     // if a drive separator is present
        p = q + 1;                        //  object name may start after drive separator
    if ((q = strrchr(p, '/')) != NULL)    // If there's any path separator in the string
        p = q + 1;                        //  object name starts after last path separator
    limit -= reserve;                     // lower limit by reserved character space
    overflow = strlen(p) - limit;         // Calculate length of string to remove (if positive)
    if ((limit <= 0) || (overflow <= 0))  // if limit invalid, or not exceeded
        goto limited;                     //  no further limitation is needed
    if ((q = strrchr(p, '.')) == NULL)    // if there's no extension separator
        goto limit_end;                   // limitation must be done at end of full name
    r = q - overflow;                     // r is the place to recopy file extension
    if (r > p) {                          // if this place is above string start
        strcpy(r, q);                     // remove overflow from end of prefix part
        goto limited;                     // which concludes the limitation
    }                                     // if we fall through here, the prefix part was too short for the limitation needed
limit_end:
    p[limit] = 0;  //  remove overflow from end of full name
limited:

    if (folder_flag)            // if original path ended with path separator
        strcat(uLE_path, "/");  //  reappend final path separator after name
}
//------------------------------
// endfunc genLimObjName
//--------------------------------------------------------------
int genFixPath(const char *inp_path, char *gen_path)
{
    char uLE_path[MAX_PATH], loc_path[MAX_PATH], party[MAX_NAME], *p;
    char *pathSep;
    int part_ix;

    part_ix = 99;  // Assume valid non-HDD path
    if (!uLE_related(uLE_path, inp_path))
        part_ix = -99;           // Assume invalid uLE_related path
    strcpy(gen_path, uLE_path);  // Assume no path patching needed
    pathSep = strchr(uLE_path, '/');

    if (!strncmp(uLE_path, "cdfs", 4)) {  // if using CD or DVD disc path
        // TODO: Flush CDFS cache
        sceCdDiskReady(0);
        // end of clause for using a CD or DVD path

    } else if (!strncmp(uLE_path, "mass", 4)) {  // if using USB mass: path
        if (pathSep && (pathSep - uLE_path < 7) && pathSep[-1] == ':')
            strcpy(gen_path + (pathSep - uLE_path), pathSep + 1);
        // end of clause for using a USB mass: path

    } else if (!strncmp(uLE_path, "hdd0:/", 6)) {  // If using HDD path
        // Get path on HDD unit, LaunchELF's format (e.g. hdd0:/partition/path/to/file)
        strcpy(loc_path, uLE_path + 6);
        if ((p = strchr(loc_path, '/')) != NULL) {
            // Extract path to file within partition. Make a new path, relative to the filesystem root.
            // hdd0:/partition/path/to/file becomes pfs0:/path/to/file.
            sprintf(gen_path, "pfs0:%s", p);
            *p = 0;  // null-terminate the block device path (hdd0:/partition).
        } else {
            // Otherwise, default to /
            strcpy(gen_path, "pfs0:/");
        }
        // Generate standard path to the block device (i.e. hdd0:/partition results in hdd0:partition)
        sprintf(party, "hdd0:%s", loc_path);
        if (nparties == 0) {
            // No partitions recognized? Load modules & populate partition list.
            loadHddModules();
            setPartyList();
        }
        // Mount the partition.
        if ((part_ix = mountParty(party)) >= 0)
            gen_path[3] = part_ix + '0';
        // end of clause for using an HDD path
    } else if (!strncmp(uLE_path, "dvr_hdd0:/", 10)) {  // If using DVRP HDD path
        // Get path on DVR HDD unit, LaunchELF's format (e.g. dvr_hdd0:/partition/path/to/file)
        strcpy(loc_path, uLE_path + 10);
        if ((p = strchr(loc_path, '/')) != NULL) {
            // Extract path to file within partition. Make a new path, relative to the filesystem root.
            // dvr_hdd0:/partition/path/to/file becomes dvr_pfs0:/path/to/file.
            sprintf(gen_path, "dvr_pfs0:%s", p);
            *p = 0;  // null-terminate the block device path (dvr_hdd0:/partition).
        } else {
            // Otherwise, default to /
            strcpy(gen_path, "dvr_pfs0:/");
        }
        // Generate standard path to the block device (i.e. dvr_hdd0:/partition results in hdd0:partition)
        sprintf(party, "dvr_hdd0:%s", loc_path);
        if (ndvrpparties == 0) {
            // No partitions recognized? Load modules & populate partition list.
            loadDVRPHddModules();
            setDVRPPartyList();
        }
        // Mount the partition.
        if ((part_ix = mountDVRPParty(party)) >= 0)
            gen_path[7] = part_ix + '0';
        // end of clause for using an HDD path
    }
    genLimObjName(gen_path, 0);
    return part_ix;
    // non-HDD Path => 99, Good HDD Path => 0-3, Bad Path => negative
}
//------------------------------
// endfunc genFixPath
//--------------------------------------------------------------
int genRmdir(char *path)
{
    int ret;

    genLimObjName(path, 0);
    ret = fileXioRmdir(path);
    if (!strncmp(path, "vmc", 3))
        fileXioDevctl("vmc0:", DEVCTL_VMCFS_CLEAN, NULL, 0, NULL, 0);
    return ret;
}
//------------------------------
// endfunc genRmdir
//--------------------------------------------------------------
int genRemove(char *path)
{
    int ret;

    genLimObjName(path, 0);
    ret = fileXioRemove(path);
    if (!strncmp(path, "vmc", 3))
        fileXioDevctl("vmc0:", DEVCTL_VMCFS_CLEAN, NULL, 0, NULL, 0);
    return ret;
}
//------------------------------
// endfunc genRemove
//--------------------------------------------------------------
int genOpen(char *path, int mode)
{
    genLimObjName(path, 0);
    return open(path, mode, fileMode);
}
//------------------------------
// endfunc genOpen
//--------------------------------------------------------------
int genDopen(char *path)
{
    int fd;

    if (!strncmp(path, "pfs", 3) || !strncmp(path, "vmc", 3)) {
        char tmp[MAX_PATH];

        strcpy(tmp, path);
        if (tmp[strlen(tmp) - 1] == '/')
            tmp[strlen(tmp) - 1] = '\0';
        fd = fileXioDopen(tmp);
    } else {
        fd = fileXioDopen(path);
    }

    return fd;
}
//------------------------------
// endfunc genDopen
//--------------------------------------------------------------
s64 genLseek(int fd, s64 where, int how)
{
    s64 res = lseek64(fd, where, how);
    if (res == -48) {
        res = lseek(fd, where, how);
    }
    return res;
}
//------------------------------
// endfunc genLseek
//--------------------------------------------------------------
int genRead(int fd, void *buf, int size)
{
    return read(fd, buf, size);
}
//------------------------------
// endfunc genRead
//--------------------------------------------------------------
int genWrite(int fd, void *buf, int size)
{
    return write(fd, buf, size);
}
//------------------------------
// endfunc genWrite
//--------------------------------------------------------------
int genClose(int fd)
{
    return close(fd);
}
//------------------------------
// endfunc genClose
//--------------------------------------------------------------
int genDclose(int fd)
{
    return fileXioDclose(fd);
}
//------------------------------
// endfunc genDclose
//--------------------------------------------------------------
int genCmpFileExt(const char *filename, const char *extension)
{
    const char *p;

    p = strrchr(filename, '.');
    return (p != NULL && !strcasecmp(p + 1, extension));
}
//------------------------------
// endfunc genDclose
//--------------------------------------------------------------
int readVMC(const char *path, FILEINFO *info, int max)
{
    iox_dirent_t dirbuf;
    char dir[MAX_PATH];
    int i = 0, fd;

    strcpy(dir, path);
    if ((fd = fileXioDopen(dir)) < 0)
        return 0;

    while (fileXioDread(fd, &dirbuf) > 0) {
        //		if(dirbuf.stat.mode & FIO_S_IFDIR &&  //NB: normal usage (non-vmcfs)
        if (dirbuf.stat.mode & sceMcFileAttrSubdir &&  // NB: nonstandard usage of vmcfs
            (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
            continue;  // Skip pseudopaths "." and ".."

        strcpy(info[i].name, dirbuf.name);
        clear_mcTable(&info[i].stats);
        //		if(dirbuf.stat.mode & FIO_S_IFDIR){  //NB: normal usage (non-vmcfs)
        //			info[i].stats.attrFile = MC_ATTR_norm_folder;
        //		}
        if (dirbuf.stat.mode & sceMcFileAttrSubdir) {  // NB: vmcfs usage
            info[i].stats.AttrFile = dirbuf.stat.mode;
        }
        //		else if(dirbuf.stat.mode & FIO_S_IFREG){  //NB: normal usage (non-vmcfs)
        //			info[i].stats.attrFile = MC_ATTR_norm_file;
        //			info[i].stats.fileSizeByte = dirbuf.stat.size;
        //			info[i].stats.unknown4[0] = dirbuf.stat.hisize;
        //		}
        else if (dirbuf.stat.mode & sceMcFileAttrFile) {  // NB: vmcfs usage
            info[i].stats.AttrFile = dirbuf.stat.mode;
            info[i].stats.FileSizeByte = dirbuf.stat.size;
            info[i].stats.Reserve2 = dirbuf.stat.hisize;
        } else
            continue;  // Skip entry which is neither a file nor a folder
        memcpy((char *)info[i].stats.EntryName, info[i].name, 32);
        info[i].stats.EntryName[sizeof(info[i].stats.EntryName) - 1] = 0;
        memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
        memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
        i++;
        if (i == max)
            break;
    }

    fileXioDclose(fd);

    size_valid = 1;
    time_valid = 1;

    return i;
}
//------------------------------
// endfunc readVMC
//--------------------------------------------------------------
int readHDD(const char *path, FILEINFO *info, int max)
{
    iox_dirent_t dirbuf;
    char party[MAX_PATH], dir[MAX_PATH];
    int i = 0, fd, ret;

    if (nparties == 0) {
        loadHddModules();
        setPartyList();
    }

    if (!strcmp(path, "hdd0:/")) {
        for (i = 0; i < nparties; i++) {
            strcpy(info[i].name, parties[i]);
            info[i].stats.AttrFile = MC_ATTR_norm_folder;
        }
        return nparties;
    }

    getHddParty(path, NULL, party, dir);
    ret = mountParty(party);
    if (ret < 0)
        return 0;
    dir[3] = ret + '0';

    if ((fd = fileXioDopen(dir)) < 0)
        return 0;

    while (fileXioDread(fd, &dirbuf) > 0) {
        if (dirbuf.stat.mode & FIO_S_IFDIR &&
            (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
            continue;  // Skip pseudopaths "." and ".."

        strcpy(info[i].name, dirbuf.name);
        clear_mcTable(&info[i].stats);
        if (dirbuf.stat.mode & FIO_S_IFDIR) {
            info[i].stats.AttrFile = MC_ATTR_norm_folder;
        } else if (dirbuf.stat.mode & FIO_S_IFREG) {
            info[i].stats.AttrFile = MC_ATTR_norm_file;
            info[i].stats.FileSizeByte = dirbuf.stat.size;
            info[i].stats.Reserve2 = dirbuf.stat.hisize;
        } else
            continue;  // Skip entry which is neither a file nor a folder
        memcpy((char *)info[i].stats.EntryName, info[i].name, 32);
        info[i].stats.EntryName[sizeof(info[i].stats.EntryName) - 1] = 0;
        memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
        memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
        i++;
        if (i == max)
            break;
    }

    fileXioDclose(fd);

    size_valid = 1;
    time_valid = 1;

    return i;
}
//------------------------------
// endfunc readHDD
//--------------------------------------------------------------
int readHDDDVRP(const char *path, FILEINFO *info, int max)
{
    iox_dirent_t dirbuf;
    char party[MAX_PATH], dir[MAX_PATH];
    int i = 0, fd, ret;

    if (ndvrpparties == 0) {
        loadDVRPHddModules();
        setDVRPPartyList();
    }

    if (!strcmp(path, "dvr_hdd0:/")) {
        for (i = 0; i < ndvrpparties; i++) {
            strcpy(info[i].name, parties[i]);
            info[i].stats.AttrFile = MC_ATTR_norm_folder;
        }
        return ndvrpparties;
    }

    getHddDVRPParty(path, NULL, party, dir);
    ret = mountDVRPParty(party);
    if (ret < 0)
        return 0;
    dir[7] = ret + '0';

    if ((fd = fileXioDopen(dir)) < 0)
        return 0;

    while (fileXioDread(fd, &dirbuf) > 0) {
        if (dirbuf.stat.mode & FIO_S_IFDIR &&
            (!strcmp(dirbuf.name, ".") || !strcmp(dirbuf.name, "..")))
            continue;  // Skip pseudopaths "." and ".."

        strcpy(info[i].name, dirbuf.name);
        clear_mcTable(&info[i].stats);
        if (dirbuf.stat.mode & FIO_S_IFDIR) {
            info[i].stats.AttrFile = MC_ATTR_norm_folder;
        } else if (dirbuf.stat.mode & FIO_S_IFREG) {
            info[i].stats.AttrFile = MC_ATTR_norm_file;
            info[i].stats.FileSizeByte = dirbuf.stat.size;
            info[i].stats.Reserve2 = dirbuf.stat.hisize;
        } else
            continue;  // Skip entry which is neither a file nor a folder
        memcpy((char *)info[i].stats.EntryName, info[i].name, 32);
        info[i].stats.EntryName[sizeof(info[i].stats.EntryName) - 1] = 0;
        memcpy((void *)&info[i].stats._Create, dirbuf.stat.ctime, 8);
        memcpy((void *)&info[i].stats._Modify, dirbuf.stat.mtime, 8);
        i++;
        if (i == max)
            break;
    }

    fileXioDclose(fd);

    size_valid = 1;
    time_valid = 1;

    return i;
}
//------------------------------
// endfunc readHDDDVRP
//--------------------------------------------------------------
void scan_USB_mass(void)
{
    int i;
    iox_stat_t chk_stat;
    char mass_path[8] = "mass0:/";

    if ((USB_mass_max_drives < 2)  // No need for dynamic lists with only one drive
        || (USB_mass_scanned && ((Timer() - USB_mass_scan_time) < 5000)))
        return;

    for (i = 0; i < USB_mass_max_drives; i++) {
        mass_path[4] = '0' + i;
        if (fileXioGetStat(mass_path, &chk_stat) < 0) {
            USB_mass_ix[i] = 0;
            continue;
        }
        USB_mass_ix[i] = '0' + i;
        USB_mass_scanned = 1;
        USB_mass_scan_time = Timer();
    }  // ends for loop
}
//------------------------------
// endfunc scan_USB_mass
//--------------------------------------------------------------
int readMASS(const char *path, FILEINFO *info, int max)
{
    iox_dirent_t record;
    int n = 0, dd = -1;

    if (!USB_mass_scanned)
        scan_USB_mass();

    if ((dd = fileXioDopen(path)) < 0)
        goto exit;  // exit if error opening directory
    while (fileXioDread(dd, &record) > 0) {
        if ((FIO_S_ISDIR(record.stat.mode)) && (!strcmp(record.name, ".") || !strcmp(record.name, "..")))
            continue;  // Skip entry if pseudo-folder "." or ".."

        strcpy(info[n].name, record.name);
        clear_mcTable(&info[n].stats);
        if (FIO_S_ISDIR(record.stat.mode)) {
            info[n].stats.AttrFile = MC_ATTR_norm_folder;
        } else if (FIO_S_ISREG(record.stat.mode)) {
            info[n].stats.AttrFile = MC_ATTR_norm_file;
            info[n].stats.FileSizeByte = record.stat.size;
            info[n].stats.Reserve2 = record.stat.hisize;
        } else
            continue;  // Skip entry which is neither a file nor a folder
        memcpy((char *)info[n].stats.EntryName, info[n].name, 32);
        info[n].stats.EntryName[sizeof(info[n].stats.EntryName) - 1] = 0;
        memcpy((void *)&info[n].stats._Create, record.stat.ctime, 8);
        memcpy((void *)&info[n].stats._Modify, record.stat.mtime, 8);
        n++;
        if (n == max)
            break;
    }  // ends while
    size_valid = 1;
    time_valid = 1;

exit:
    if (dd >= 0)
        fileXioDclose(dd);  // Close directory if opened above
    return n;
}
//------------------------------
// endfunc readMASS
//--------------------------------------------------------------
char *makeHostPath(char *dp, char *sp)
{
    int i;
    char ch;

    if (!host_use_Bsl)
        return strcpy(dp, sp);

    for (i = 0; i < MAX_PATH - 1; i++) {
        ch = sp[i];
        if (ch == '/')
            dp[i] = '\\';
        else
            dp[i] = ch;
        if (!ch)
            break;
    }
    return dp;
}
//--------------------------------------------------------------
char *makeFslPath(char *dp, const char *sp)
{
    int i;
    char ch;

    for (i = 0; i < MAX_PATH - 1; i++) {
        ch = sp[i];
        if (ch == '\\')
            dp[i] = '/';
        else
            dp[i] = ch;
        if (!ch)
            break;
    }
    return dp;
}
//--------------------------------------------------------------
void initHOST(void)
{
    int fd;

    load_ps2host();
    host_error = 0;
    if ((fd = open("host:elflist.txt", O_RDONLY)) >= 0) {
        close(fd);
        host_elflist = 1;
    } else {
        host_elflist = 0;
        if ((fd = fileXioDopen("host:")) >= 0)
            fileXioDclose(fd);
        else
            host_error = 1;
    }
    host_ready = 1;
}
//--------------------------------------------------------------
int readHOST(const char *path, FILEINFO *info, int max)
{
    iox_dirent_t hostcontent;
    int hfd, rv, size, contentptr, hostcount = 0;
    char *elflisttxt, elflistchar;
    char host_path[MAX_PATH * 2], host_next[MAX_PATH];
    char Win_path[MAX_PATH];

    initHOST();
    snprintf(host_path, MAX_PATH - 1, "%s", path);
    if (!strncmp(path, "host:/", 6))
        strcpy(host_path + 5, path + 6);
    if ((host_elflist) && !strcmp(host_path, "host:")) {
        if ((hfd = open("host:elflist.txt", O_RDONLY, 0)) < 0)
            return 0;
        if ((size = lseek(hfd, 0, SEEK_END)) <= 0) {
            close(hfd);
            return 0;
        }
        elflisttxt = (char *)memalign(64, size);
        lseek(hfd, 0, SEEK_SET);
        read(hfd, elflisttxt, size);
        close(hfd);
        contentptr = 0;
        for (rv = 0; rv <= size; rv++) {
            elflistchar = elflisttxt[rv];
            if ((elflistchar == 0x0a) || (rv == size)) {
                host_next[contentptr] = 0;
                snprintf(host_path, sizeof(host_path), "%s%s", "host:", host_next);
                clear_mcTable(&info[hostcount].stats);
                if ((hfd = open(makeHostPath(Win_path, host_path), O_RDONLY)) >= 0) {
                    close(hfd);
                    info[hostcount].stats.AttrFile = MC_ATTR_norm_file;
                    makeFslPath(info[hostcount++].name, host_next);
                } else if ((hfd = fileXioDopen(Win_path)) >= 0) {
                    fileXioDclose(hfd);
                    info[hostcount].stats.AttrFile = MC_ATTR_norm_folder;
                    makeFslPath(info[hostcount++].name, host_next);
                }
                contentptr = 0;
                if (hostcount > max)
                    break;
            } else if (elflistchar != 0x0d) {
                host_next[contentptr] = elflistchar;
                contentptr++;
            }
        }
        free(elflisttxt);
        return hostcount - 1;
    }
    // This point is only reached if elflist.txt is NOT to be used

    if ((hfd = fileXioDopen(makeHostPath(Win_path, host_path))) < 0)
        return 0;
    strcpy(host_path, Win_path);
    while ((rv = fileXioDread(hfd, &hostcontent))) {
        if (strcmp(hostcontent.name, ".") && strcmp(hostcontent.name, "..")) {
            size_valid = 1;
            time_valid = 1;
            snprintf(Win_path, MAX_PATH - 1, "%s%s", host_path, hostcontent.name);
            strcpy(info[hostcount].name, hostcontent.name);
            clear_mcTable(&info[hostcount].stats);

            if (!(hostcontent.stat.mode & FIO_S_IFDIR))  // if not a directory
                info[hostcount].stats.AttrFile = MC_ATTR_norm_file;
            else
                info[hostcount].stats.AttrFile = MC_ATTR_norm_folder;

            info[hostcount].stats.FileSizeByte = hostcontent.stat.size;
            info[hostcount].stats.Reserve2 = hostcontent.stat.hisize;  // taking an unused(?) unknown for the high bits
            memcpy((void *)&info[hostcount].stats._Create, hostcontent.stat.ctime, 8);
            info[hostcount].stats._Create.Year += 1900;
            memcpy((void *)&info[hostcount].stats._Modify, hostcontent.stat.mtime, 8);
            info[hostcount].stats._Modify.Year += 1900;
            hostcount++;
            if (hostcount >= max)
                break;
        }
    }
    fileXioDclose(hfd);
    strcpy(info[hostcount].name, "\0");
    return hostcount;
}
//------------------------------
// endfunc readHOST
//--------------------------------------------------------------
int getDir(const char *path, FILEINFO *info)
{
    int max = MAX_ENTRY - 2;
    int n;

    if (!strncmp(path, "mc", 2))
        n = readMC(path, info, max);
    else if (!strncmp(path, "hdd", 3))
        n = readHDD(path, info, max);
    else if (!strncmp(path, "dvr_hdd", 7))
        n = readHDDDVRP(path, info, max);
    else if (!strncmp(path, "mass", 4))
        n = readMASS(path, info, max);
    else if (!strncmp(path, "cdfs", 4))
        n = readCD(path, info, max);
    else if (!strncmp(path, "host", 4))
        n = readHOST(path, info, max);
    else if (!strncmp(path, "vmc", 3))
        n = readVMC(path, info, max);
    else
        return 0;

    return n;
}
//--------------------------------------------------------------
// getGameTitle below is used to extract the real save title of
// an MC gamesave folder. Normally this is held in the icon.sys
// file of a PS2 game save, but that is not the case for saves
// on a PS1 MC, or similar saves backed up to a PS2 MC. Two new
// methods need to be used to extract such titles correctly, and
// these were added in v3.62, by me (dlanor).
// From v3.91 This routine also extracts titles from PSU files.
//--------------------------------------------------------------
static int getGameTitle(const char *path, const FILEINFO *file, unsigned char *out)
{
    char party[MAX_NAME], dir[MAX_PATH], tmpdir[MAX_PATH];
    int fd = -1, size, ret;
    psu_header PSU_head;
    int i, tst, PSU_content, psu_pad_pos;

    out[0] = '\0';  // Start by making an empty result string, for failures

    // Avoid title usage in browser root or partition list
    if (path[0] == 0 || !strcmp(path, "hdd0:/") || !strcmp(path, "dvr_hdd0:/"))
        return -1;

    if (!strncmp(path, "hdd", 3)) {
        ret = getHddParty(path, file, party, dir);
        if (mountParty(party) < 0)
            return -1;
        dir[3] = ret + '0';
    } else if (!strncmp(path, "dvr_hdd", 7)) {
        ret = getHddDVRPParty(path, file, party, dir);
        if (mountDVRPParty(party) < 0)
            return -1;
        dir[7] = ret + '0';
    } else {
        strcpy(dir, path);
        strcat(dir, file->name);
        if (file->stats.AttrFile & sceMcFileAttrSubdir)
            strcat(dir, "/");
    }

    ret = -1;  // Assume that result will be failure, to simplify aborts

    if ((file->stats.AttrFile & sceMcFileAttrSubdir) == 0) {
        // Here we know that the object needing a title is a file
        strcpy(tmpdir, dir);                // Copy the pathname for file access
        if (!genCmpFileExt(tmpdir, "psu"))  // Find the extension, if any. If it's anything other than a PSU file
            goto get_PS1_GameTitle;         // then it may be a PS1 save
        // Here we know that the object needing a title is a PSU file
        if ((fd = genOpen(tmpdir, O_RDONLY)) < 0)
            goto finish;  // Abort if open fails
        tst = genRead(fd, (void *)&PSU_head, sizeof(PSU_head));
        if (tst != sizeof(PSU_head))
            goto finish;  // Abort if read fails
        PSU_content = PSU_head.size;
        for (i = 0; i < PSU_content; i++) {
            tst = genRead(fd, (void *)&PSU_head, sizeof(PSU_head));
            if (tst != sizeof(PSU_head))
                goto finish;  // Abort if read fails
            PSU_head.name[sizeof(PSU_head.name) - 1] = '\0';
            if (!strcmp((char *)PSU_head.name, "icon.sys")) {
                genLseek(fd, 0xC0, SEEK_CUR);
                goto read_title;
            }
            if (PSU_head.size) {
                psu_pad_pos = (PSU_head.size + 0x3FF) & -0x400;
                genLseek(fd, psu_pad_pos, SEEK_CUR);
            }
            // Here the PSU file pointer is positioned for reading next header
        }  // ends for
        // Coming here means that the search for icon.sys failed
        goto finish;  // So go finish off this function
    }                 // ends if clause for files needing a title

    // Here we know that the object needing a title is a folder
    // First try to find a valid PS2 icon.sys file inside the folder
    strcpy(tmpdir, dir);
    strcat(tmpdir, "icon.sys");
    if ((fd = genOpen(tmpdir, O_RDONLY)) >= 0) {
        if ((size = (int)genLseek(fd, 0, SEEK_END)) <= 0x100)
            goto finish;
        genLseek(fd, 0xC0, SEEK_SET);
        goto read_title;
    }
    // Next try to find a valid PS1 savefile inside the folder instead
    strcpy(tmpdir, dir);
    strcat(tmpdir, file->name);  // PS1 save file should have same name as folder

get_PS1_GameTitle:
    if ((fd = genOpen(tmpdir, O_RDONLY)) < 0)
        goto finish;  // PS1 gamesave file needed
    if ((size = (int)genLseek(fd, 0, SEEK_END)) < 0x2000)
        goto finish;  // Min size is 8K
    if (size & 0x1FFF)
        goto finish;  // Size must be a multiple of 8K
    genLseek(fd, 0, SEEK_SET);
    genRead(fd, out, 2);
    if (strncmp((const char *)out, "SC", 2))
        goto finish;  // PS1 gamesaves always start with "SC"
    genLseek(fd, 4, SEEK_SET);

read_title:
    genRead(fd, out, 32 * 2);
    out[32 * 2] = 0;
    genClose(fd);
    fd = -1;
    ret = 0;

finish:
    if (fd >= 0)
        genClose(fd);
    return ret;
}
//--------------------------------------------------------------
int menu(const char *path, FILEINFO *file)
{
    u64 color;
    char enable[NUM_MENU], tmp[80];
    int x, y, i, sel;
    int event, post_event = 0;
    int menu_disabled = 0;
    int write_disabled = 0;

    int menu_len = strlen(LNG(Copy)) > strlen(LNG(Cut)) ?
                       strlen(LNG(Copy)) :
                       strlen(LNG(Cut));
    menu_len = strlen(LNG(Paste)) > menu_len ? strlen(LNG(Paste)) : menu_len;
    menu_len = strlen(LNG(Delete)) > menu_len ? strlen(LNG(Delete)) : menu_len;
    menu_len = strlen(LNG(Rename)) > menu_len ? strlen(LNG(Rename)) : menu_len;
    menu_len = strlen(LNG(New_Dir)) > menu_len ? strlen(LNG(New_Dir)) : menu_len;
    menu_len = strlen(LNG(Get_Size)) > menu_len ? strlen(LNG(Get_Size)) : menu_len;
    menu_len = strlen(LNG(mcPaste)) > menu_len ? strlen(LNG(mcPaste)) : menu_len;
    menu_len = strlen(LNG(psuPaste)) > menu_len ? strlen(LNG(psuPaste)) : menu_len;
    menu_len = (strlen(LNG(Mount)) + 6) > menu_len ? (strlen(LNG(Mount)) + 6) : menu_len;

    int menu_ch_w = menu_len + 1;                                 // Total characters in longest menu string
    int menu_ch_h = NUM_MENU;                                     // Total number of menu lines
    int mSprite_Y1 = 64;                                          // Top edge of sprite
    int mSprite_X2 = SCREEN_WIDTH - 35;                           // Right edge of sprite
    int mSprite_X1 = mSprite_X2 - (menu_ch_w + 3) * FONT_WIDTH;   // Left edge of sprite
    int mSprite_Y2 = mSprite_Y1 + (menu_ch_h + 1) * FONT_HEIGHT;  // Bottom edge of sprite

    memset(enable, TRUE, NUM_MENU);  // Assume that all menu items are legal by default

    // identify cases where write access is illegal, and disable menu items accordingly
    if ((!strncmp(path, "cdfs", 4))                           // Writing is always illegal for CDVD drive
        || ((!strncmp(path, "host", 4))                       // host: has special cases
            && ((!setting->HOSTwrite)                         // host: Writing is illegal if not enabled in CNF
                || (host_elflist && !strcmp(path, "host:/"))  // it's also illegal in elflist.txt
                )))
        write_disabled = 1;

    if ((!strcmp(path, "hdd0:/") || !strcmp(path, "dvr_hdd0:/")) || path[0] == 0)  // No menu cmds in partition/device lists
        menu_disabled = 1;

    if (menu_disabled) {
        enable[COPY] = FALSE;
        enable[MOUNTVMC0] = FALSE;
        enable[MOUNTVMC1] = FALSE;
        enable[GETSIZE] = FALSE;
    }

    if (write_disabled || menu_disabled) {
        enable[CUT] = FALSE;
        enable[PASTE] = FALSE;
        enable[MCPASTE] = FALSE;
        enable[PSUPASTE] = FALSE;
        enable[DELETE] = FALSE;
        enable[RENAME] = FALSE;
        enable[NEWDIR] = FALSE;
        enable[NEWICON] = FALSE;
    }

    if (nmarks == 0) {
        if (!strcmp(file->name, "..")) {
            enable[COPY] = FALSE;
            enable[CUT] = FALSE;
            enable[DELETE] = FALSE;
            enable[RENAME] = FALSE;
            enable[GETSIZE] = FALSE;
        }
    } else {
        enable[RENAME] = FALSE;
    }

    if ((file->stats.AttrFile & sceMcFileAttrSubdir) || !strncmp(path, "vmc", 3) || !strncmp(path, "mc", 2)) {
        enable[MOUNTVMC0] = FALSE;  // forbid insane VMC mounting
        enable[MOUNTVMC1] = FALSE;  // forbid insane VMC mounting
    }

    if (nclipFiles == 0) {
        // Nothing in clipboard
        enable[PASTE] = FALSE;
        enable[MCPASTE] = FALSE;
        enable[PSUPASTE] = FALSE;
    } else {
        // Something in clipboard
        if (!strncmp(path, "mc", 2) || !strncmp(path, "vmc", 3)) {
            if (!strncmp(clipPath, "mc", 2) || !strncmp(clipPath, "vmc", 3)) {
                enable[MCPASTE] = FALSE;  // No mcPaste if both src and dest are MC
                enable[PSUPASTE] = FALSE;
            }
        } else if (strncmp(clipPath, "mc", 2) && strncmp(clipPath, "vmc", 3)) {
            enable[MCPASTE] = FALSE;  // No mcPaste if both src and dest non-MC
            enable[PSUPASTE] = FALSE;
        }
    }

    for (sel = 0; sel < NUM_MENU; sel++)  // loop to preselect the first enabled menu entry
        if (enable[sel] == TRUE)
            break;  // break loop if sel is at an enabled menu entry

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
                event |= 2;  // event |= valid pad command
                break;
            } else if (new_pad & PAD_SQUARE && sel == PASTE) {
                event |= 2;  // event |= valid pad command
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
                if (i == COPY)
                    strcpy(tmp, LNG(Copy));
                else if (i == CUT)
                    strcpy(tmp, LNG(Cut));
                else if (i == PASTE)
                    strcpy(tmp, LNG(Paste));
                else if (i == MCPASTE)
                    strcpy(tmp, LNG(mcPaste));
                else if (i == PSUPASTE)
                    strcpy(tmp, LNG(psuPaste));
                else if (i == DELETE)
                    strcpy(tmp, LNG(Delete));
                else if (i == RENAME)
                    strcpy(tmp, LNG(Rename));
                else if (i == NEWDIR)
                    strcpy(tmp, LNG(New_Dir));
                else if (i == NEWICON)
                    strcpy(tmp, LNG(New_Icon));
                else if (i == MOUNTVMC0)
                    sprintf(tmp, "%s vmc0:", LNG(Mount));
                else if (i == MOUNTVMC1)
                    sprintf(tmp, "%s vmc1:", LNG(Mount));
                else if (i == GETSIZE)
                    strcpy(tmp, LNG(Get_Size));

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
                       SCREEN_WIDTH, y + FONT_HEIGHT);
            if (swapKeys)
                sprintf(tmp, "\xFF"
                             "1:%s \xFF"
                             "0:%s",
                        LNG(OK), LNG(Cancel));
            else
                sprintf(tmp, "\xFF"
                             "0:%s \xFF"
                             "1:%s",
                        LNG(OK), LNG(Cancel));
            if (sel == PASTE)
                sprintf(tmp + strlen(tmp), " \xFF"
                                           "2:%s",
                        LNG(PasteRename));
            sprintf(tmp + strlen(tmp), " \xFF"
                                       "3:%s",
                    LNG(Back));
            printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while
    return sel;
}  // ends menu
//--------------------------------------------------------------
char *PathPad_menu(const char *path)
{
    u64 color;
    int x, y, dx, dy, dw, dh;
    int a = 6, b = 4, c = 2, tw, th;
    int i, sel_x, sel_y;
    int event, post_event = 0;
    char textrow[80], tmp[64];

    th = 10 * FONT_HEIGHT;          // Height in pixels of text area
    tw = 68 * FONT_WIDTH;           // Width in pixels of max text row
    dh = th + 2 * 2 + a + b + c;    // Height in pixels of entire frame
    dw = tw + 2 * 2 + a * 2;        // Width in pixels of entire frame
    dx = (SCREEN_WIDTH - dw) / 2;   // X position of frame (centred)
    dy = (SCREEN_HEIGHT - dh) / 2;  // Y position of frame (centred)

    sel_x = 0;
    sel_y = 0;
    event = 1;  // event = initial entry
    while (1) {
        // Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad) {
                event |= 2;  // event |= valid pad command
                if (new_pad & PAD_UP) {
                    sel_y--;
                    if (sel_y < 0)
                        sel_y = 9;
                } else if (new_pad & PAD_DOWN) {
                    sel_y++;
                    if (sel_y > 9)
                        sel_y = 0;
                } else if (new_pad & PAD_LEFT) {
                    sel_y -= 5;
                    if (sel_y < 0)
                        sel_y = 0;
                } else if (new_pad & PAD_RIGHT) {
                    sel_y += 5;
                    if (sel_y > 9)
                        sel_y = 9;
                } else if (new_pad & PAD_L1) {
                    sel_x--;
                    if (sel_x < 0)
                        sel_x = 2;
                } else if (new_pad & PAD_R1) {
                    sel_x++;
                    if (sel_x > 2)
                        sel_x = 0;
                } else if (new_pad & PAD_TRIANGLE) {  // Pushed 'Back'
                    return NULL;
                } else if (!setting->PathPad_Lock                                                            // if PathPad changes allowed ?
                           && ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE))) {  // Pushed 'Clear'
                    PathPad[sel_x * 10 + sel_y][0] = '\0';
                } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  // Pushed 'Use'
                    return PathPad[sel_x * 10 + sel_y];
                } else if (!setting->PathPad_Lock && (new_pad & PAD_SQUARE)) {  // Pushed 'Set'
                    strncpy(PathPad[sel_x * 10 + sel_y], path, MAX_PATH - 1);
                    PathPad[sel_x * 10 + sel_y][MAX_PATH - 1] = '\0';
                }
            }  // ends 'if(new_pad)'
        }      // ends 'if(readpad())'

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            drawSprite(setting->color[COLOR_BACKGR],
                       0, (Menu_message_y - 1),
                       SCREEN_WIDTH, (Frame_start_y));
            drawPopSprite(setting->color[COLOR_BACKGR],
                          dx, dy,
                          dx + dw, (dy + dh));
            drawFrame(dx, dy, dx + dw, (dy + dh), setting->color[COLOR_FRAME]);
            for (i = 0; i < 10; i++) {
                if (i == sel_y)
                    color = setting->color[COLOR_SELECT];
                else
                    color = setting->color[COLOR_TEXT];
                sprintf(textrow, "%02d=", (sel_x * 10 + i));
                strncat(textrow, PathPad[sel_x * 10 + i], 64);
                textrow[67] = '\0';
                printXY(textrow, dx + 2 + a, (dy + a + 2 + i * FONT_HEIGHT), color, TRUE, 0);
            }

            // Tooltip section
            x = SCREEN_MARGIN;
            y = Menu_tooltip_y;
            drawSprite(setting->color[COLOR_BACKGR], 0, y - 1, SCREEN_WIDTH, y + FONT_HEIGHT);

            if (swapKeys) {
                sprintf(textrow, "\xFF"
                                 "1:%s ",
                        LNG(Use));
                if (!setting->PathPad_Lock) {
                    sprintf(tmp, "\xFF"
                                 "0:%s \xFF"
                                 "2:%s ",
                            LNG(Clear), LNG(Set));
                    strcat(textrow, tmp);
                }
            } else {
                sprintf(textrow, "\xFF"
                                 "0:%s ",
                        LNG(Use));
                if (!setting->PathPad_Lock) {
                    sprintf(tmp, "\xFF"
                                 "1:%s \xFF"
                                 "2:%s ",
                            LNG(Clear), LNG(Set));
                    strcat(textrow, tmp);
                }
            }
            sprintf(tmp, "\xFF"
                         "3:%s L1/R1:%s",
                    LNG(Back), LNG(Page_leftright));
            strcat(textrow, tmp);
            printXY(textrow, x, y, setting->color[COLOR_SELECT], TRUE, 0);
        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while
}
//------------------------------
// endfunc PathPad_menu
//--------------------------------------------------------------
u64 getFileSize(const char *path, const FILEINFO *file)
{
    iox_stat_t stat;
    u64 size, filesize;
    FILEINFO files[MAX_ENTRY];
    char dir[MAX_PATH], party[MAX_NAME];
    int nfiles, i, ret;

    if (file->stats.AttrFile & sceMcFileAttrSubdir) {  // Folder object to size up
        sprintf(dir, "%s%s/", path, file->name);
        nfiles = getDir(dir, files);
        for (i = size = 0; i < nfiles; i++) {
            filesize = getFileSize(dir, &files[i]);  // recurse for each object in folder
            if (filesize < 0)
                return -1;
            else
                size += filesize;
        }
    } else {  // File object to size up
        if (!strncmp(path, "hdd", 3)) {
            getHddParty(path, file, party, dir);
            ret = mountParty(party);
            if (ret < 0)
                return 0;
            dir[3] = ret + '0';
        }
        if (!strncmp(path, "dvr_hdd", 7)) {
            getHddDVRPParty(path, file, party, dir);
            ret = mountDVRPParty(party);
            if (ret < 0)
                return 0;
            dir[7] = ret + '0';
        } else
            sprintf(dir, "%s%s", path, file->name);
        if (!strncmp(dir, "host:/", 6))
            makeHostPath(dir + 5, dir + 6);

        fileXioGetStat(dir, &stat);
        size = stat.size;
    }
    return size;
}
//------------------------------
// endfunc getFileSize
//--------------------------------------------------------------
int delete (const char *path, const FILEINFO *file)
{
    FILEINFO files[MAX_ENTRY];
    char party[MAX_NAME], dir[MAX_PATH], hdddir[MAX_PATH];
    int nfiles, i, ret;

    if (!strncmp(path, "hdd", 3)) {
        getHddParty(path, file, party, hdddir);
        ret = mountParty(party);
        if (ret < 0)
            return 0;
        hdddir[3] = ret + '0';
    } else if (!strncmp(path, "dvr_hdd", 7)) {
        getHddDVRPParty(path, file, party, hdddir);
        ret = mountDVRPParty(party);
        if (ret < 0)
            return 0;
        hdddir[7] = ret + '0';
    }
    sprintf(dir, "%s%s", path, file->name);
    genLimObjName(dir, 0);
    if (!strncmp(dir, "host:/", 6))
        makeHostPath(dir + 5, dir + 6);

    if (file->stats.AttrFile & sceMcFileAttrSubdir) {  // Is the object to delete a folder ?
        strcat(dir, "/");
        nfiles = getDir(dir, files);
        for (i = 0; i < nfiles; i++) {
            ret = delete (dir, &files[i]);  // recursively delete contents of folder
            if (ret < 0)
                return -1;
        }
        if (!strncmp(dir, "mc", 2)) {
            mcSync(0, NULL, NULL);
            mcDelete(dir[2] - '0', 0, &dir[4]);
            mcSync(0, NULL, &ret);

        } else if (!strncmp(path, "hdd", 3) || !strncmp(path, "dvr_hdd", 7)) {
            ret = rmdir(hdddir);

        } else if (!strncmp(path, "vmc", 3)) {
            ret = rmdir(dir);
            fileXioDevctl("vmc0:", DEVCTL_VMCFS_CLEAN, NULL, 0, NULL, 0);

        } else {  // For all other devices
            sprintf(dir, "%s%s", path, file->name);
            ret = rmdir(dir);
        }
    } else {  // The object to delete is a file
        if (!strncmp(path, "mc", 2)) {
            mcSync(0, NULL, NULL);
            mcDelete(dir[2] - '0', 0, &dir[4]);
            mcSync(0, NULL, &ret);
        } else if (!strncmp(path, "hdd", 3) || !strncmp(path, "dvr_hdd", 7)) {
            ret = unlink(hdddir);
        } else if (!strncmp(path, "vmc", 3)) {
            ret = unlink(dir);
            fileXioDevctl("vmc0:", DEVCTL_VMCFS_CLEAN, NULL, 0, NULL, 0);
        } else {  // For all other devices
            ret = unlink(dir);
        }
    }
    return ret;
}
//--------------------------------------------------------------
int Rename(const char *path, const FILEINFO *file, const char *name)
{
    char party[MAX_NAME], oldPath[MAX_PATH], newPath[MAX_PATH];
    int test, ret = 0;

    if (!strncmp(path, "hdd", 3)) {
        sprintf(party, "hdd0:%s", &path[6]);
        *strchr(party, '/') = 0;
        sprintf(oldPath, "pfs0:%s", strchr(&path[6], '/') + 1);
        sprintf(newPath, "%s%s", oldPath, name);
        strcat(oldPath, file->name);

        ret = mountParty(party);
        if (ret < 0)
            return -1;
        oldPath[3] = newPath[3] = ret + '0';

        ret = fileXioRename(oldPath, newPath);
    } else if (!strncmp(path, "dvr_hdd", 7)) {
        sprintf(party, "dvr_hdd0:%s", &path[10]);
        *strchr(party, '/') = 0;
        sprintf(oldPath, "dvr_pfs0:%s", strchr(&path[10], '/') + 1);
        sprintf(newPath, "%s%s", oldPath, name);
        strcat(oldPath, file->name);

        ret = mountDVRPParty(party);
        if (ret < 0)
            return -1;
        oldPath[7] = newPath[7] = ret + '0';

        ret = fileXioRename(oldPath, newPath);
    } else if (!strncmp(path, "mc", 2)) {
        sprintf(oldPath, "%s%s", path, file->name);
        sprintf(newPath, "%s%s", path, name);
        if ((test = fileXioDopen(newPath)) >= 0) {  // Does folder of same name exist ?
            fileXioDclose(test);
            ret = -EEXIST;
        } else if ((test = open(newPath, O_RDONLY)) >= 0) {  // Does file of same name exist ?
            close(test);
            ret = -EEXIST;
        } else {  // No file/folder of the same name exists
            mcGetInfo(path[2] - '0', 0, &mctype_PSx, NULL, NULL);
            mcSync(0, NULL, &test);
            if (mctype_PSx == 2) {  // PS2 MC ?
                memcpy((void *)file->stats.EntryName, name, 32);
            }
            mcSetFileInfo(path[2] - '0', 0, oldPath + 4, &file->stats, 0x0010);  // Fix file stats
            mcSync(0, NULL, &test);
            if (ret == -4)
                ret = -EEXIST;
            else {  // PS1 MC !
                memcpy((void *)file->stats.EntryName, name, 32);
                mcSetFileInfo(path[2] - '0', 0, oldPath + 4, &file->stats, 0x0010);  // Fix file stats
                mcSync(0, NULL, &test);
            }
        }
    } else if (!strncmp(path, "host", 4)) {
        int temp_fd;

        strcpy(oldPath, path);
        if (!strncmp(oldPath, "host:/", 6))
            makeHostPath(oldPath + 5, oldPath + 6);
        strcpy(newPath, oldPath + 5);
        strcat(oldPath, file->name);
        strcat(newPath, name);
        if (file->stats.AttrFile & sceMcFileAttrSubdir) {  // Rename a folder ?
            ret = (temp_fd = fileXioDopen(oldPath));
            if (temp_fd >= 0) {
                ret = fileXioIoctl(temp_fd, IOCTL_RENAME, (void *)newPath);
                fileXioDclose(temp_fd);
            }
        } else if (file->stats.AttrFile & sceMcFileAttrFile) {  // Rename a file ?
            ret = (temp_fd = open(oldPath, O_RDONLY));
            if (temp_fd >= 0) {
                ret = _ps2sdk_ioctl(temp_fd, IOCTL_RENAME, (void *)newPath);
                close(temp_fd);
            }
        } else  // This was neither a folder nor a file !!!
            return -1;
    } else {  // For all other devices
        sprintf(oldPath, "%s%s", path, file->name);
        sprintf(newPath, "%s%s", path, name);
        ret = fileXioRename(oldPath, newPath);
    }

    return ret;
}

//--------------------------------------------------------------
int newdir(const char *path, const char *name)
{
    char party[MAX_NAME], dir[MAX_PATH];
    int ret = 0;

    if (!strncmp(path, "hdd", 3)) {
        getHddParty(path, NULL, party, dir);
        ret = mountParty(party);
        if (ret < 0)
            return -1;
        dir[3] = ret + '0';
        strcat(dir, name);
        genLimObjName(dir, 0);
        ret = fileXioMkdir(dir, fileMode);
    } else if (!strncmp(path, "dvr_hdd", 7)) {
        getHddDVRPParty(path, NULL, party, dir);
        ret = mountDVRPParty(party);
        if (ret < 0)
            return -1;
        dir[7] = ret + '0';
        strcat(dir, name);
        genLimObjName(dir, 0);
        ret = fileXioMkdir(dir, fileMode);
    } else if (!strncmp(path, "vmc", 3)) {
        strcpy(dir, path);
        strcat(dir, name);
        genLimObjName(dir, 0);
        if ((ret = fileXioDopen(dir)) >= 0) {
            fileXioDclose(ret);
            ret = -EEXIST;  // return fileXio error code for pre-existing folder
        } else {
            ret = fileXioMkdir(dir, fileMode);
        }
    } else if (!strncmp(path, "mc", 2)) {
        sprintf(dir, "%s%s", path + 4, name);
        genLimObjName(dir, 0);
        mcSync(0, NULL, NULL);
        mcMkDir(path[2] - '0', 0, dir);
        mcSync(0, NULL, &ret);
        if (ret == -4)
            ret = -EEXIST;  // return fileXio error code for pre-existing folder
    } else if (!strncmp(path, "host", 4)) {
        strcpy(dir, path);
        strcat(dir, name);
        genLimObjName(dir, 0);
        if (!strncmp(dir, "host:/", 6))
            makeHostPath(dir + 5, dir + 6);
        if ((ret = fileXioDopen(dir)) >= 0) {
            fileXioDclose(ret);
            ret = -EEXIST;  // return fileXio error code for pre-existing folder
        } else {
            ret = fileXioMkdir(dir, fileMode);  // Create the new folder
        }
    } else {  // For all other devices
        strcpy(dir, path);
        strcat(dir, name);
        genLimObjName(dir, 0);
        ret = fileXioMkdir(dir, fileMode);
    }
    return ret;
}
//--------------------------------------------------------------
// The function 'copy' below is called to copy a single object,
// indicated by 'file' from 'inPath' to 'outPath', but this may
// be either a single file or a folder. In the latter case the
// folder contents should also be copied, recursively.
//--------------------------------------------------------------
int copy(char *outPath, const char *inPath, FILEINFO file, int recurses)
{
    FILEINFO newfile, files[MAX_ENTRY];
    iox_stat_t iox_stat;
    char out[MAX_PATH], in[MAX_PATH], tmp[MAX_PATH * 2],
        progress[MAX_PATH * 4],
        *buff = NULL, inParty[MAX_NAME], outParty[MAX_NAME];
    int nfiles, i;
    s64 size;
    int ret = -1, pfsout = -1, pfsin = -1, in_fd = -1, out_fd = -1, buffSize, bytesRead, bytesWritten;
    int dummy;
    sceMcTblGetDir stats;
    int speed = 0;
    int remain_time = 0, TimeDiff = 0;
    long old_size = 0, SizeDiff = 0;
    u64 OldTime = 0LL;
    psu_header PSU_head;
    mcT_header *mcT_head_p = (mcT_header *)&file.stats;
    mcT_header *mcT_files_p = (mcT_header *)&files[0].stats;
    int psu_pad_size = 0, PSU_restart_f = 0;
    char *cp, *np;

    PM_flag[recurses + 1] = PM_NORMAL;  // assume normal mode for next level
    PM_file[recurses + 1] = -1;         // assume that no special file is needed

restart_copy:  // restart point for PM_PSU_RESTORE to reprocess modified arguments

    newfile = file;  // assume that no renaming is to be done

    if (PasteMode == PM_RENAME && recurses == 0) {  // if renaming requested and valid
        if (keyboard(newfile.name, 36) <= 0)        // if name entered by user made the result invalid
            strcpy(newfile.name, file.name);        //  recopy newname from file.name
    }                                               // ends if clause for renaming name entry
    // Here the struct 'newfile' is FILEINFO for destination, regardless of renaming
    // for non-renaming cases this is always identical to the struct 'file'

    if (!strncmp(inPath, "hdd", 3)) {
        getHddParty(inPath, &file, inParty, in);
        pfsin = mountParty(inParty);
        in[3] = pfsin + '0';
    }
    if (!strncmp(inPath, "dvr_hdd", 7)) {
        getHddDVRPParty(inPath, &file, inParty, in);
        pfsin = mountDVRPParty(inParty);
        in[7] = pfsin + '0';
    } else
        sprintf(in, "%s%s", inPath, file.name);

    if (!strncmp(outPath, "hdd", 3)) {
        getHddParty(outPath, &newfile, outParty, out);
        pfsout = mountParty(outParty);
        out[3] = pfsout + '0';
    } else if (!strncmp(outPath, "dvr_hdd", 7)) {
        getHddDVRPParty(outPath, &newfile, outParty, out);
        pfsout = mountDVRPParty(outParty);
        out[7] = pfsout + '0';
    } else
        sprintf(out, "%s%s", outPath, newfile.name);

    if (!strcmp(in, out))
        return 0;  // if in and out are identical our work is done.

    // Here 'in' and 'out' are complete pathnames for the object to copy
    // patched to contain appropriate 'pfs' refs where args used 'hdd'
    // The physical device specifiers remain in 'inPath' and 'outPath'

    // Here we have an object to copy, which may be either a file or a folder
    if (file.stats.AttrFile & sceMcFileAttrSubdir) {
        // Here we have a folder to copy, starting with an attempt to create it
        // This is where we must act differently for PSU backup, creating a PSU file instead
        if (PasteMode == PM_PSU_BACKUP) {
            if (recurses)
                return -1;  // abort, as subfolders are not copied to PSU backups
            i = strlen(out) - 1;
            if (out[i] == '/')
                out[i] = 0;
            strcpy(tmp, out);
            np = tmp + strlen(tmp) - strlen(file.name);  // np = start of the pure filename
            cp = tmp + strlen(tmp);                      // cp = end of the pure filename
            if (!setting->PSU_HugeNames)
                cp = np;  // cp = start of the pure filename

            if ((file_show == 2) || setting->PSU_HugeNames) {  // at request, use game title
                ret = getGameTitle(inPath, &file, file.title);
                if ((ret == 0) && file.title[0] && setting->PSU_HugeNames) {
                    *cp++ = '_';
                    *cp = '\0';
                }
                transcpy_sjis(cp, file.title);
            }
            // Here game title has been used for the name if requested, either alone
            // or combined with the original folder name (for PSU_HugeNames)

            if (np[0] == 0)             // If name is now empty (bad gamesave title)
                strcpy(np, file.name);  // revert to normal folder name

            for (i = 0; np[i];) {
                i = strcspn(np, "\"\\/:*?<>|");  // Filter out illegal name characters
                if (np[i])
                    np[i] = '_';
            }
            // Here illegal characters, from either title or original folder name
            // have been filtered out (replaced by underscore) to ensure compatibility

            cp = tmp + strlen(tmp);        // set cp pointing to the end of the filename
            if (setting->PSU_DateNames) {  // at request, append modification timestamp string
                sprintf(cp, "_%04d-%02d-%02d_%02d-%02d-%02d",
                        mcT_head_p->mTime.year, mcT_head_p->mTime.month, mcT_head_p->mTime.day,
                        mcT_head_p->mTime.hour, mcT_head_p->mTime.min, mcT_head_p->mTime.sec);
            }
            // Here a timestamp has been added to the name if requested by PSU_DateNames

            genLimObjName(tmp, 4);  // Limit name to leave room for 4 characters more
            strcat(tmp, ".psu");    // add the PSU file extension

            if (!strncmp(tmp, "host:/", 6))
                makeHostPath(tmp + 5, tmp + 6);

            if (setting->PSU_DateNames && setting->PSU_NoOverwrite) {
                if (0 <= (out_fd = genOpen(tmp, O_RDONLY))) {  // Name conflict ?
                    genClose(out_fd);
                    out_fd = -1;
                    return 0;
                }
            }
            // here tmp is the name of an existing file, to be removed before making new one
            genRemove(tmp);
            if (0 > (out_fd = genOpen(tmp, O_WRONLY | O_TRUNC | O_CREAT)))
                return -1;  // return error on failure to create PSU file

            PM_file[recurses + 1] = out_fd;
            PM_flag[recurses + 1] = PM_PSU_BACKUP;  // Set PSU backup mode for next level
            clear_psu_header(&PSU_head);
            PSU_content = 2;  // 2 content headers minimum, for empty PSU
            PSU_head.attr = mcT_head_p->attr;
            PSU_head.size = PSU_content;
            PSU_head.cTime = mcT_head_p->cTime;
            PSU_head.mTime = mcT_head_p->mTime;
            memcpy(PSU_head.name, mcT_head_p->name, sizeof(PSU_head.name));
            PSU_head.unknown_1_u16 = mcT_head_p->unknown_1_u16;
            PSU_head.unknown_2_u64 = mcT_head_p->unknown_2_u64;
            size = genWrite(out_fd, (void *)&PSU_head, sizeof(PSU_head));
            if (size != sizeof(PSU_head)) {
            PSU_error:
                genClose(PM_file[recurses + 1]);
                return -1;
            }
            clear_psu_header(&PSU_head);
            PSU_head.attr = 0x8427;  // Standard folder attr, for pseudo "." and ".."
            PSU_head.cTime = mcT_head_p->cTime;
            PSU_head.mTime = mcT_head_p->mTime;
            PSU_head.name[0] = '.';  // Set name entry to "."
            size = genWrite(out_fd, (void *)&PSU_head, sizeof(PSU_head));
            if (size != sizeof(PSU_head))
                goto PSU_error;
            PSU_head.name[1] = '.';  // Change name entry to ".."
            size = genWrite(out_fd, (void *)&PSU_head, sizeof(PSU_head));
            if (size != sizeof(PSU_head))
                goto PSU_error;
        } else {  // any other PasteMode than PM_PSU_BACKUP
            ret = newdir(outPath, newfile.name);
            if (ret == -17) {  // NB: 'newdir' must return -17 for ALL pre-existing folder cases
                ret = getGameTitle(outPath, &newfile, newfile.title);
                transcpy_sjis(tmp, newfile.title);
                sprintf(progress,
                        "%s:\n"
                        "%s%s/\n"
                        "\n"
                        "%s:\n",
                        LNG(Name_conflict_for_this_folder), outPath,
                        file.name, LNG(With_gamesave_title));
                if (tmp[0])
                    strcat(progress, tmp);
                else
                    strcat(progress, LNG(Undefined_Not_a_gamesave));
                sprintf(tmp, "\n\n%s ?", LNG(Do_you_wish_to_overwrite_it));
                strcat(progress, tmp);
                if (ynDialog(progress) < 0)
                    return -1;
                if ((PasteMode == PM_MC_BACKUP) || (PasteMode == PM_MC_RESTORE) || (PasteMode == PM_PSU_RESTORE)) {
                    ret = delete (outPath, &newfile);  // Attempt recursive delete
                    if (ret < 0)
                        return -1;
                    if (newdir(outPath, newfile.name) < 0)
                        return -1;
                }
                drawMsg(LNG(Pasting));
            } else if (ret < 0) {
                return -1;  // return error for failure to create destination folder
            }
        }
        // Here a destination folder, or a PSU file exists, ready to receive files
        if (PasteMode == PM_MC_BACKUP) {  // MC Backup mode folder paste preparation
            sprintf(tmp, "%s/PS2_MC_Backup_Attributes.BUP.bin", out);
            genRemove(tmp);
            out_fd = genOpen(tmp, O_WRONLY | O_CREAT);

            if (out_fd >= 0) {
                size = genWrite(out_fd, (void *)&file.stats, 64);
                if (size == 64) {
                    PM_file[recurses + 1] = out_fd;
                    PM_flag[recurses + 1] = PM_MC_BACKUP;
                } else
                    genClose(PM_file[recurses + 1]);
            }
        } else if (PasteMode == PM_MC_RESTORE) {  // MC Restore mode folder paste preparation
            sprintf(tmp, "%s/PS2_MC_Backup_Attributes.BUP.bin", in);

            if (!strncmp(tmp, "host:/", 6))
                makeHostPath(tmp + 5, tmp + 6);
            in_fd = genOpen(tmp, O_RDONLY);

            if (in_fd >= 0) {
                size = genRead(in_fd, (void *)&file.stats, 64);  // Read stats for the save folder
                if (size == 64) {
                    PM_file[recurses + 1] = in_fd;
                    PM_flag[recurses + 1] = PM_MC_RESTORE;
                } else
                    genClose(PM_file[recurses + 1]);
            }
        }
        if (PM_flag[recurses + 1] == PM_NORMAL) {  // Normal mode folder paste preparation
        }
        sprintf(in, "%s%s/", inPath, file.name);      // restore phys dev spec to 'in'
        sprintf(out, "%s%s", outPath, newfile.name);  // restore phys dev spec to 'out'
        genLimObjName(out, 0);                        // Limit dest folder name
        strcat(out, "/");                             // Separate dest folder name

        if (PasteMode == PM_PSU_RESTORE && PSU_restart_f) {
            nfiles = PSU_content;
            for (i = 0; i < nfiles; i++) {
                size = genRead(PM_file[recurses + 1], (void *)&PSU_head, sizeof(PSU_head));
                if (size != sizeof(PSU_head)) {  // Break with error on read failure
                    ret = -1;
                    break;
                }
                if ((PSU_head.size == 0) && (PSU_head.attr & sceMcFileAttrSubdir))  // Dummy/Pseudo folder entry ?
                    continue;                                                       // Just ignore dummies
                if (PSU_head.attr & sceMcFileAttrSubdir) {                          // break with error on weird folder in PSU
                    ret = -1;
                    break;
                }
                if (PSU_head.size & 0x3FF)  // Check if file is padded in PSU
                    psu_pad_size = 0x400 - (PSU_head.size & 0x3FF);
                else
                    psu_pad_size = 0;
                // here we need to create a proper FILEINFO struct for PSU-contained file
                mcT_files_p->attr = PSU_head.attr;
                mcT_files_p->size = PSU_head.size;
                mcT_files_p->cTime = PSU_head.cTime;
                mcT_files_p->mTime = PSU_head.mTime;
                memcpy(mcT_files_p->name, PSU_head.name, sizeof(PSU_head.name));
                mcT_files_p->unknown_1_u16 = PSU_head.unknown_1_u16;
                mcT_files_p->unknown_2_u64 = PSU_head.unknown_2_u64;
                memcpy(files[0].name, PSU_head.name, sizeof(PSU_head.name));
                files[0].name[sizeof(PSU_head.name)] = 0;
                // Finally we can make the recursive call
                if ((ret = copy(out, in, files[0], recurses + 1)) < 0)
                    break;
                // We must also step past any file padding, for next header
                if (psu_pad_size)
                    genLseek(PM_file[recurses + 1], psu_pad_size, SEEK_CUR);
                // finally, we must adjust attributes of the new file copy, to ensure
                // correct timestamps and attributes (requires MC-specific functions)
                strcpy(tmp, out);
                strncat(tmp, (const char *)files[0].stats.EntryName, 32);
                mcGetInfo(tmp[2] - '0', 0, &dummy, &dummy, &dummy);  // Wakeup call
                mcSync(0, NULL, &dummy);
                mcSetFileInfo(tmp[2] - '0', 0, &tmp[4], &files[0].stats, MC_SFI);  // Fix file stats
                mcSync(0, NULL, &dummy);
            }                                 // ends main for loop of valid PM_PSU_RESTORE mode
            genClose(PM_file[recurses + 1]);  // Close the PSU file
                                              // Finally fix the stats of the containing folder
                                              // It has to be done last, as timestamps would change when fixing files
                                              //--- This has been moved to a later clause, shared with PM_MC_RESTORE ---
        } else {                              // Any other mode than a valid PM_PSU_RESTORE
            nfiles = getDir(in, files);
            for (i = 0; i < nfiles; i++) {
                if ((ret = copy(out, in, files[i], recurses + 1)) < 0)
                    break;
            }  // ends main for loop for all modes other than valid PM_PSU_RESTORE
        }
        // folder contents are copied by the recursive call above, with error handling below
        if (ret < 0) {
            if (PM_flag[recurses + 1] == PM_PSU_BACKUP)
                goto PSU_error;
            else
                return -1;
        }

        // Here folder contents have been copied error-free, but we also need to copy
        // attributes and timestamps, and close the attribute/PSU file if such was used
        // Lots of stuff need to be done here to make PSU operations work properly
        if (PM_flag[recurses + 1] == PM_MC_BACKUP) {  // MC Backup mode folder paste closure
            genClose(PM_file[recurses + 1]);
        } else if (PM_flag[recurses + 1] == PM_PSU_BACKUP) {  // PSU Backup mode folder paste closure
            genLseek(PM_file[recurses + 1], 0, SEEK_SET);
            clear_psu_header(&PSU_head);
            PSU_head.attr = mcT_head_p->attr;
            PSU_head.size = PSU_content;
            PSU_head.cTime = mcT_head_p->cTime;
            PSU_head.mTime = mcT_head_p->mTime;
            memcpy(PSU_head.name, mcT_head_p->name, sizeof(PSU_head.name));
            PSU_head.unknown_1_u16 = mcT_head_p->unknown_1_u16;
            PSU_head.unknown_2_u64 = mcT_head_p->unknown_2_u64;
            size = genWrite(PM_file[recurses + 1], (void *)&PSU_head, sizeof(PSU_head));
            genLseek(PM_file[recurses + 1], 0, SEEK_END);
            genClose(PM_file[recurses + 1]);
        } else if (PM_flag[recurses + 1] == PM_MC_RESTORE) {  // MC Restore mode folder paste closure
            in_fd = PM_file[recurses + 1];
            for (size = 64, i = 0; size == 64;) {
                size = genRead(in_fd, (void *)&stats, 64);
                if (size == 64) {
                    strcpy(tmp, out);
                    strncat(tmp, (const char *)stats.EntryName, 32);
                    mcGetInfo(tmp[2] - '0', 0, &dummy, &dummy, &dummy);  // Wakeup call
                    mcSync(0, NULL, &dummy);
                    mcSetFileInfo(tmp[2] - '0', 0, &tmp[4], &stats, MC_SFI);  // Fix file stats
                    mcSync(0, NULL, &dummy);
                } else {
                    genClose(in_fd);
                }
            }
            // Finally fix the stats of the containing folder
            // It has to be done last, as timestamps would change when fixing files
            //--- This has been moved to a later clause, shared with PM_PSU_RESTORE ---
        } else if (PM_flag[recurses + 1] == PM_NORMAL) {             // Normal mode folder paste closure
            if (!strncmp(out, "mc", 2)) {                            // Handle folder copied to MC
                mcGetInfo(out[2] - '0', 0, &dummy, &dummy, &dummy);  // Wakeup call
                mcSync(0, NULL, &dummy);
                ret = MC_SFI;                                   // default request for changing entire mcTable
                if (strncmp(in, "mc", 2)) {                     // Handle file copied from non-MC to MC
                    file.stats.AttrFile = MC_ATTR_norm_folder;  // normalize MC folder attribute
                    if (!strncmp(in, "host", 4)) {              // Handle folder copied from host: to MC
                        ret = 4;                                // request change only of main attribute for host:
                    }                                           // ends host: source clause
                }                                               // ends non-MC source clause
                mcSetFileInfo(out[2] - '0', 0, &out[4], &file.stats, ret);
                mcSync(0, NULL, &dummy);
            } else {                                    // Handle folder copied to non-MC
                if (!strncmp(out, "host", 4)) {         // for files copied to host: we skip Chstat
                } else if (!strncmp(out, "mass", 4)) {  // for files copied to mass: we skip Chstat
                } else {                                // for other devices we use fileXio_ stuff
                    memcpy(iox_stat.ctime, (void *)&file.stats._Create, 8);
                    memcpy(iox_stat.mtime, (void *)&file.stats._Modify, 8);
                    memcpy(iox_stat.atime, iox_stat.mtime, 8);
                    ret = FIO_CST_CT | FIO_CST_AT | FIO_CST_MT;  // Request timestamp stat change
                    if (!strncmp(in, "host", 4)) {               // Handle folder copied from host:
                        ret = 0;                                 // Request NO stat change
                    }
                    dummy = fileXioChStat(out, &iox_stat, ret);
                }
            }
        }
        if ((PM_flag[recurses + 1] == PM_MC_RESTORE) || (PM_flag[recurses + 1] == PM_PSU_RESTORE)) {
            // Finally fix the stats of the containing folder
            // It has to be done last, as timestamps would change when fixing files
            mcGetInfo(out[2] - '0', 0, &dummy, &dummy, &dummy);  // Wakeup call
            mcSync(0, NULL, &dummy);
            mcSetFileInfo(out[2] - '0', 0, &out[4], &file.stats, MC_SFI);  // Fix folder stats
            mcSync(0, NULL, &dummy);
        }
        // the return code below is used if there were no errors copying a folder
        return 0;
    }

    // Here we know that the object to copy is a file, not a folder
    // But in PSU Restore mode we must treat PSU files as special folders, at level 0.
    // and recursively call copy with higher recurse level to process the contents
    if (PasteMode == PM_PSU_RESTORE && recurses == 0) {
        if (!genCmpFileExt(in, "psu"))
            goto non_PSU_RESTORE_init;  // if not a PSU file, go do normal pasting

        in_fd = genOpen(in, O_RDONLY);

        if (in_fd < 0)
            return -1;
        size = genRead(in_fd, (void *)&PSU_head, sizeof(PSU_head));
        if (size != sizeof(PSU_head)) {
            genClose(in_fd);
            return -1;
        }
        PM_file[recurses + 1] = in_fd;           // File descriptor for PSU
        PM_flag[recurses + 1] = PM_PSU_RESTORE;  // Mode flag for recursive entry
        // Here we need to prep the file struct to appear like a normal MC folder
        // before 'restarting' this 'copy' to handle creation of destination folder
        // as well as the copying of files from the PSU into that folder
        mcT_head_p->attr = PSU_head.attr;
        PSU_content = PSU_head.size;
        mcT_head_p->size = 0;
        mcT_head_p->cTime = PSU_head.cTime;
        mcT_head_p->mTime = PSU_head.mTime;
        memcpy(mcT_head_p->name, PSU_head.name, sizeof(PSU_head.name));
        mcT_head_p->unknown_1_u16 = PSU_head.unknown_1_u16;
        mcT_head_p->unknown_2_u64 = PSU_head.unknown_2_u64;
        memcpy(file.name, PSU_head.name, sizeof(PSU_head.name));
        file.name[sizeof(PSU_head.name)] = 0;
        PSU_restart_f = 1;
        goto restart_copy;
    }
non_PSU_RESTORE_init:
    // In MC Restore mode we must here avoid copying the attribute file
    if (PM_flag[recurses] == PM_MC_RESTORE)
        if (!strcmp(file.name, "PS2_MC_Backup_Attributes.BUP.bin"))
            return 0;

    // It is now time to open the input file, indicated by 'in_fd'
    // But in PSU Restore mode we must use the already open PSU file instead
    if (PM_flag[recurses] == PM_PSU_RESTORE) {
        in_fd = PM_file[recurses];
        size = mcT_head_p->size;
    } else {  // Any other mode than PM_PSU_RESTORE
        if (!strncmp(in, "host:/", 6))
            makeHostPath(in + 5, in + 6);
        in_fd = genOpen(in, O_RDONLY);
        if (in_fd < 0)
            goto copy_file_exit;
        size = genLseek(in_fd, 0, SEEK_END);
        genLseek(in_fd, 0, SEEK_SET);
    }

    // Here the input file has been opened, indicated by 'in_fd'
    // It is now time to open the output file, indicated by 'out_fd'
    // except in the case of a PSU backup, when we must add a header to PSU instead
    if (PM_flag[recurses] == PM_PSU_BACKUP) {
        out_fd = PM_file[recurses];
        clear_psu_header(&PSU_head);
        PSU_head.attr = mcT_head_p->attr;
        PSU_head.size = mcT_head_p->size;
        PSU_head.cTime = mcT_head_p->cTime;
        PSU_head.mTime = mcT_head_p->mTime;
        memcpy(PSU_head.name, mcT_head_p->name, sizeof(PSU_head.name));
        genWrite(out_fd, (void *)&PSU_head, sizeof(PSU_head));
        if (PSU_head.size & 0x3FF)
            psu_pad_size = 0x400 - (PSU_head.size & 0x3FF);
        else
            psu_pad_size = 0;
        PSU_content++;  // Increase PSU content header count
    } else {            // Any other PasteMode than PM_PSU_BACKUP needs a new output file
        if (!strncmp(out, "host:/", 6))
            makeHostPath(out + 5, out + 6);
        genLimObjName(out, 0);                                // Limit dest file name
        genRemove(out);                                       // Remove old file if present
        out_fd = genOpen(out, O_WRONLY | O_TRUNC | O_CREAT);  // Create new file
        if (out_fd < 0)
            goto copy_file_exit;
    }

    // Here the output file has been opened, indicated by 'out_fd'

    /* Determine the block size. Since LaunchELF is single-threaded,
       using a large block size with a slow device will result in an unresponsive UI.
       To prevent a loss in performance, these values must each be in a multiple of the device's sector/page size.
       They must also be in multiples of 64, to prevent FILEIO from doing alignment correction in software. */
    buffSize = 0x100000;  // First assume buffer size = 1MB (good for HDD)
    if (!strncmp(out, "mc", 2) || !strncmp(out, "mass", 4) || !strncmp(out, "vmc", 3))
        buffSize = 131072;  // Use  128KB if writing to USB (Flash RAM writes) or MC (pretty slow).
                            // VMC contents should use the same size, as VMCs will often be stored on USB
    else if (!strncmp(in, "mc", 2))
        buffSize = 262144;  // Use 256KB if reading from MC (still pretty slow)
    else if (!strncmp(out, "host", 4))
        buffSize = 393216;  // Use 384KB if writing to HOST (acceptable)
    else if ((!strncmp(in, "mass", 4)) || (!strncmp(in, "host", 4)))
        buffSize = 524288;  // Use 512KB reading from USB or HOST (acceptable)

    if (size < buffSize)
        buffSize = size;

    buff = (char *)memalign(64, buffSize);  // Attempt buffer allocation
    if (buff == NULL) {                     // if allocation fails
        ret = -ENOMEM;
        genClose(out_fd);
        out_fd = -1;
        if (PM_flag[recurses] != PM_PSU_BACKUP)
            genRemove(out);
        goto copy_file_exit_mem_err;
    }

    old_size = written_size;  // Note initial progress data pos
    OldTime = Timer();        // Note initial progress time

    while (size > 0) {  // ----- The main copying loop starts here -----

        if (size < buffSize)
            buffSize = size;  // Adjust effective buffer size to remaining data

        TimeDiff = Timer() - OldTime;
        OldTime = Timer();
        SizeDiff = written_size - old_size;
        old_size = written_size;
        if (SizeDiff) {                            // if anything was written this time
            speed = (SizeDiff * 1000) / TimeDiff;  // calc real speed
            remain_time = size / speed;            // calc time remaining for that speed
        } else if (TimeDiff) {                     // if nothing written though time passed
            speed = 0;                             // set speed as zero
            remain_time = -1;                      // set time remaining as unknown
        } else {                                   // if nothing written and no time passed
            speed = -1;                            // set speed as unknown
            remain_time = -1;                      // set time remaining as unknown
        }

        sprintf(progress, "%s : %s", LNG(Pasting_file), file.name);

        sprintf(tmp, "\n%s : ", LNG(Remain_Size));
        strcat(progress, tmp);
        if (size <= 1024)
            sprintf(tmp, "%lu %s", (unsigned long)size, LNG(bytes));  // bytes
        else
            sprintf(tmp, "%lu %s", (unsigned long)size / 1024, LNG(Kbytes));  // Kbytes
        strcat(progress, tmp);

        sprintf(tmp, "\n%s: ", LNG(Current_Speed));
        strcat(progress, tmp);
        if (speed == -1)
            strcpy(tmp, LNG(Unknown));
        else if (speed <= 1024)
            sprintf(tmp, "%d %s/sec", speed, LNG(bytes));  // bytes/sec
        else
            sprintf(tmp, "%d %s/sec", speed / 1024, LNG(Kbytes));  // Kbytes/sec
        strcat(progress, tmp);

        sprintf(tmp, "\n%s : ", LNG(Remain_Time));
        strcat(progress, tmp);
        if (remain_time == -1)
            strcpy(tmp, LNG(Unknown));
        else if (remain_time < 60)
            sprintf(tmp, "%d sec", remain_time);  // sec
        else if (remain_time < 3600)
            sprintf(tmp, "%d m %d sec", remain_time / 60, remain_time % 60);  // min,sec
        else
            sprintf(tmp, "%d h %d m %d sec", remain_time / 3600, (remain_time % 3600) / 60, remain_time % 60);  // hour,min,sec
        strcat(progress, tmp);

        sprintf(tmp, "\n\n%s: ", LNG(Written_Total));
        strcat(progress, tmp);
        sprintf(tmp, "%lu %s", written_size / 1024, LNG(Kbytes));  // Kbytes
        strcat(progress, tmp);

        sprintf(tmp, "\n%s: ", LNG(Average_Speed));
        strcat(progress, tmp);
        TimeDiff = Timer() - PasteTime;
        if (TimeDiff == 0)
            strcpy(tmp, LNG(Unknown));
        else {
            speed = (written_size * 1000) / TimeDiff;  // calc real speed
            if (speed <= 1024)
                sprintf(tmp, "%d %s/sec", speed, LNG(bytes));  // bytes/sec
            else
                sprintf(tmp, "%d %s/sec", speed / 1024, LNG(Kbytes));  // Kbytes/sec
        }
        strcat(progress, tmp);

        if (PasteProgress_f)  // if progress report was used earlier in this pasting
            nonDialog(NULL);  // order cleanup for that screen area
        nonDialog(progress);  // Make new progress report
        PasteProgress_f = 1;  // and note that it was done for next time
        drawMsg(file.name);
        if (readpad() && new_pad) {
            if (-1 == ynDialog(LNG(Continue_transfer))) {
                genClose(out_fd);
                out_fd = -1;
                if (PM_flag[recurses] != PM_PSU_BACKUP)
                    genRemove(out);
                ret = -1;             // flag generic error
                goto copy_file_exit;  // go deal with it
            }
        }
        bytesRead = genRead(in_fd, buff, buffSize);
        bytesWritten = (bytesRead == buffSize) ? genWrite(out_fd, buff, buffSize) : 0;
        if ((bytesRead != buffSize) || (bytesWritten != buffSize)) {
            genClose(out_fd);
            out_fd = -1;
            if (PM_flag[recurses] != PM_PSU_BACKUP)
                genRemove(out);
            ret = -EIO;  // flag generic I/O error
            goto copy_file_exit;
        }
        size -= buffSize;
        written_size += buffSize;
    }  // ends while(size>0), ----- The main copying loop ends here -----
    ret = 0;
    // Here the file has been copied. without error, as indicated by 'ret' above
    // but we also need to copy attributes and timestamps (as yet only for MC)
    // For PSU backup output padding may be needed, but not output file closure
    if (PM_flag[recurses] == PM_PSU_BACKUP) {
        if (psu_pad_size) {
            pad_psu_header(&PSU_head);
            if (psu_pad_size >= sizeof(PSU_head)) {
                genWrite(out_fd, (void *)&PSU_head, sizeof(PSU_head));
                psu_pad_size -= sizeof(PSU_head);
            }
            if (psu_pad_size)
                genWrite(out_fd, (void *)&PSU_head, psu_pad_size);
        }
        out_fd = -1;  // prevent output file closure below
        goto copy_file_exit;
    }

    if (PM_flag[recurses] == PM_MC_BACKUP) {  // MC Backup mode file paste closure
        size = genWrite(PM_file[recurses], (void *)&file.stats, 64);
        if (size != 64)
            return -1;  // Abort if attribute file crashed
    }

    if (out_fd >= 0) {
        genClose(out_fd);
        out_fd = -1;  // prevent dual closure attempt
    }

    if (!strncmp(out, "mc", 2)) {                                 // Handle file copied to MC
        mcGetInfo(out[2] - '0', 0, &mctype_PSx, &dummy, &dummy);  // Wakeup call & MC type check
        mcSync(0, NULL, &dummy);
        ret = MC_SFI;                                 // default request for changing entire mcTable
        if (strncmp(in, "mc", 2)) {                   // Handle file copied from non-MC to MC
            file.stats.AttrFile = MC_ATTR_norm_file;  // normalize MC file attribute
            if (!strncmp(in, "host", 4)) {            // Handle folder copied from host: to MC
                ret = 4;                              // request change only of main attribute for host:
            }                                         // ends host: source clause
        }                                             // ends non-MC source clause
        if (mctype_PSx == 2) {                        // if copying to a PS2 MC
            mcSetFileInfo(out[2] - '0', 0, &out[4], &file.stats, ret);
            mcSync(0, NULL, &dummy);
        }
    } else {                                    // Handle file copied to non-MC
        if (!strncmp(out, "host", 4)) {         // for files copied to host: we skip Chstat
        } else if (!strncmp(out, "mass", 4)) {  // for files copied to mass: we skip Chstat
        } else {                                // for other devices we use fileXio_ stuff
            memcpy(iox_stat.ctime, (void *)&file.stats._Create, 8);
            memcpy(iox_stat.mtime, (void *)&file.stats._Modify, 8);
            memcpy(iox_stat.atime, iox_stat.mtime, 8);
            ret = FIO_CST_CT | FIO_CST_AT | FIO_CST_MT;  // Request timestamp stat change
            if (!strncmp(in, "host", 4)) {               // Handle file copied from host:
                ret = 0;                                 // Request NO stat change
            }
            dummy = fileXioChStat(out, &iox_stat, ret);
        }
    }

// The code below is also used for all errors in copying a file,
// but those cases are distinguished by a negative value in 'ret'
copy_file_exit:
    free(buff);
copy_file_exit_mem_err:
    if (PM_flag[recurses] != PM_PSU_RESTORE) {  // Avoid closing PSU file here for PSU Restore
        if (in_fd >= 0) {
            genClose(in_fd);
        }
    }
    if (out_fd >= 0) {
        genClose(out_fd);
    }
    return ret;
}
//------------------------------
// endfunc copy
//--------------------------------------------------------------
// dlanor: For v3.64 the virtual keyboard function is modified to
// allow entry of empty strings. The function now returns string
// length, except if you use 'CANCEL' when it returns -1 instead.
// Routines that require a non-empty string (eg: Rename, Newdir)
// must test with '>' now, instead of '>=' as used previously.
//--------------------------------------------------------------
int keyboard(char *out, int max)
{
    int event, post_event = 0;
    const int
        WFONTS = 13,
        HFONTS = 7,
        KEY_W = LINE_THICKNESS + 12 + (13 * FONT_WIDTH + 12 * 12) + 12 + LINE_THICKNESS,
        KEY_H = LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + (8 * FONT_HEIGHT) + 8 + LINE_THICKNESS,
        KEY_X = ((SCREEN_WIDTH - KEY_W) / 2) & -2,
        KEY_Y = ((SCREEN_HEIGHT - KEY_H) / 2) & -2;
    char *KEY = "ABCDEFGHIJKLM"
                "NOPQRSTUVWXYZ"
                "abcdefghijklm"
                "nopqrstuvwxyz"
                "0123456789/|\\"
                "<>(){}[].,:;\""
                "!@#$%&=+-^*_'";
    int KEY_LEN;
    int cur = 0, sel = 0, i = 0, x, y, t = 0;
    char tmp[256], *p;
    char KeyPress;

    p = strrchr(out, '.');
    if (p == NULL)
        cur = strlen(out);
    else
        cur = (int)(p - out);
    KEY_LEN = strlen(KEY);

    event = 1;  // event = initial entry
    while (1) {
        // Pad response section
        waitPadReady(0, 0);
        if (readpad_no_KB()) {
            if (new_pad)
                event |= 2;  // event |= pad command
            if (new_pad & PAD_UP) {
                if (sel <= WFONTS * HFONTS) {
                    if (sel >= WFONTS)
                        sel -= WFONTS;
                } else {
                    sel -= 4;
                }
            } else if (new_pad & PAD_DOWN) {
                if (sel / WFONTS == HFONTS - 1) {
                    if (sel % WFONTS < 5)
                        sel = WFONTS * HFONTS;
                    else
                        sel = WFONTS * HFONTS + 1;
                } else if (sel / WFONTS <= HFONTS - 2)
                    sel += WFONTS;
            } else if (new_pad & PAD_LEFT) {
                if (sel > 0)
                    sel--;
            } else if (new_pad & PAD_RIGHT) {
                if (sel <= WFONTS * HFONTS)
                    sel++;
            } else if (new_pad & PAD_START) {
                sel = WFONTS * HFONTS;
            } else if (new_pad & PAD_L1) {
                if (cur > 0)
                    cur--;
                t = 0;
            } else if (new_pad & PAD_R1) {
                if (cur < strlen(out))
                    cur++;
                t = 0;
            } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                if (cur > 0) {
                    strcpy(tmp, out);
                    out[cur - 1] = 0;
                    strcat(out, &tmp[cur]);
                    cur--;
                    t = 0;
                }
            } else if (new_pad & PAD_SQUARE) {  // Square => space
                i = strlen(out);
                if (i < max && i < 33) {
                    strcpy(tmp, out);
                    out[cur] = ' ';
                    out[cur + 1] = 0;
                    strcat(out, &tmp[cur]);
                    cur++;
                    t = 0;
                }
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                i = strlen(out);
                if (sel < WFONTS * HFONTS) {  // Any char in matrix selected ?
                    if (i < max && i < 33) {
                        strcpy(tmp, out);
                        out[cur] = KEY[sel];
                        out[cur + 1] = 0;
                        strcat(out, &tmp[cur]);
                        cur++;
                        t = 0;
                    }
                } else if (sel == WFONTS * HFONTS) {  //'OK' exit-button selected ?
                    break;                            // break out of loop with i==strlen
                } else                                // Must be 'CANCEL' exit-button
                    return -1;
            } else if (new_pad & PAD_TRIANGLE) {
                return -1;
            }
        }

        // Kbd response section
        if (setting->usbkbd_used && PS2KbdRead(&KeyPress)) {

            event |= 2;  // event |= pad command

            if (KeyPress == PS2KBD_ESCAPE_KEY) {
                PS2KbdRead(&KeyPress);
                if (KeyPress == 0x29) {  // Key Right;
                    if (cur < strlen(out))
                        cur++;
                    t = 0;
                } else if (KeyPress == 0x2a) {  // Key Left;
                    if (cur > 0)
                        cur--;
                    t = 0;
                } else if (KeyPress == 0x24) {  // Key Home;
                    cur = 0;
                    t = 0;
                } else if (KeyPress == 0x27) {  // Key End;
                    cur = strlen(out);
                    t = 0;
                } else if (KeyPress == 0x26) {  // Key Delete;
                    if (strlen(out) > cur) {
                        strcpy(tmp, out);
                        out[cur] = 0;
                        strcat(out, &tmp[cur + 1]);
                        t = 0;
                    }
                } else if (KeyPress == 0x1b) {  // Key Escape;
                    return -1;
                }
            } else {
                if (KeyPress == 0x07) {  // Key BackSpace;
                    if (cur > 0) {
                        strcpy(tmp, out);
                        out[cur - 1] = 0;
                        strcat(out, &tmp[cur]);
                        cur--;
                        t = 0;
                    }
                } else if (KeyPress == 0x0a) {  // Key Return;
                    break;
                } else {  // All Other Keys;
                    i = strlen(out);
                    if (i < max && i < 33) {
                        strcpy(tmp, out);
                        out[cur] = KeyPress;
                        out[cur + 1] = 0;
                        strcat(out, &tmp[cur]);
                        cur++;
                        t = 0;
                    }
                }
            }
            KeyPress = '\0';
        }  // ends if(setting->usbkbd_used && PS2KbdRead(&KeyPress))

        t++;

        if (t & 0x0F)
            event |= 4;  // repetitive timer event

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            drawPopSprite(setting->color[COLOR_BACKGR],
                          KEY_X, KEY_Y,
                          KEY_X + KEY_W - 1, KEY_Y + KEY_H - 1);
            drawFrame(
                KEY_X, KEY_Y,
                KEY_X + KEY_W - 1, KEY_Y + KEY_H - 1, setting->color[COLOR_FRAME]);
            drawOpSprite(setting->color[COLOR_FRAME],
                         KEY_X, KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1,
                         KEY_X + KEY_W - 1, KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS - 1);
            printXY(out, KEY_X + LINE_THICKNESS + 3, KEY_Y + LINE_THICKNESS + 1, setting->color[COLOR_TEXT], TRUE, 0);
            if (((event | post_event) & 4) && (t & 0x10)) {
                drawOpSprite(setting->color[COLOR_SELECT],
                             KEY_X + LINE_THICKNESS + 1 + cur * 8,
                             KEY_Y + LINE_THICKNESS + 2,
                             KEY_X + LINE_THICKNESS + 1 + cur * 8 + LINE_THICKNESS - 1,
                             KEY_Y + LINE_THICKNESS + 2 + (FONT_HEIGHT - 2) - 1);
            }
            for (i = 0; i < KEY_LEN; i++)
                drawChar(KEY[i],
                         KEY_X + LINE_THICKNESS + 12 + (i % WFONTS) * (FONT_WIDTH + 12),
                         KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + (i / WFONTS) * FONT_HEIGHT, setting->color[COLOR_TEXT]);
            printXY(LNG(OK),
                    KEY_X + LINE_THICKNESS + 12,
                    KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + HFONTS * FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);
            printXY(LNG(CANCEL),
                    KEY_X + KEY_W - 1 - (strlen(LNG(CANCEL)) + 2) * FONT_WIDTH,
                    KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + HFONTS * FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);

            // Cursor positioning section
            if (sel <= WFONTS * HFONTS)
                x = KEY_X + LINE_THICKNESS + 12 + (sel % WFONTS) * (FONT_WIDTH + 12) - 8;
            else
                x = KEY_X + KEY_W - 2 - (strlen(LNG(CANCEL)) + 3) * FONT_WIDTH;
            y = KEY_Y + LINE_THICKNESS + 1 + FONT_HEIGHT + 1 + LINE_THICKNESS + 8 + (sel / WFONTS) * FONT_HEIGHT;
            drawChar(LEFT_CUR, x, y, setting->color[COLOR_SELECT]);

            // Tooltip section
            x = SCREEN_MARGIN;
            y = Menu_tooltip_y;
            drawSprite(setting->color[COLOR_BACKGR], 0, y - 1, SCREEN_WIDTH, y + FONT_HEIGHT);

            if (swapKeys) {
                sprintf(tmp, "\xFF"
                             "1:%s \xFF"
                             "0",
                        LNG(Use));
            } else {
                sprintf(tmp, "\xFF"
                             "0:%s \xFF"
                             "1",
                        LNG(Use));
            }
            sprintf(tmp + strlen(tmp), ":%s \xFF"
                                       "2:%s L1:%s R1:%s START:%s \xFF"
                                       "3:%s",
                    LNG(BackSpace), LNG(SPACE), LNG(Left), LNG(Right), LNG(Enter), LNG(Exit));
            printXY(tmp, x, y, setting->color[COLOR_SELECT], TRUE, 0);
        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while
    return strlen(out);
}
//------------------------------
// endfunc keyboard
//--------------------------------------------------------------
// keyboard2 below is used for testing output from a USB keyboard
// it can be commented out when not used by the programmer.
// When using it for tests, simply replace the call to 'keyboard'
// somewhere (Rename routine is a good choice) with a call to
//'keyboard2' instead. It uses the old routines for virtual keys
// via gamepad, so you can still enter proper strings that way,
// but each key pressed on the USB keyboard will be expanded to a
// sequence corresponding to sprintf(somestring," %02X ", key).
// Thus four characters are added to the output string for each
// such key, and after character 32 the cursor loops back to the
// first character again.
//--------------------------------------------------------------
/*
int keyboard2(char *out, int max)
{
    int event, post_event=0;
    const int	KEY_W=276,
                KEY_H=168,
                KEY_X=(SCREEN_WIDTH - KEY_W)/2,
                KEY_Y=((SCREEN_HEIGHT - KEY_H)/2),
                WFONTS=13,
                HFONTS=7;
    char *KEY="ABCDEFGHIJKLM"
              "NOPQRSTUVWXYZ"
              "abcdefghijklm"
              "nopqrstuvwxyz"
              "0123456789   "
              "()[]!#$%&@;  "
              "=+-'^.,_     ";
    int KEY_LEN;
    int cur=0, sel=0, i=0, x, y, t=0;
    char tmp[256], *p;
    unsigned char KeyPress;

    p=strrchr(out, '.');
    if(p==NULL)	cur=strlen(out);
    else		cur=(int)(p-out);
    KEY_LEN = strlen(KEY);

    event = 1;  //event = initial entry
    while(1){
        //Pad response section
        waitPadReady(0, 0);
        if(readpad_no_KB()){
            if(new_pad)
                event |= 2;  //event |= pad command
            if(new_pad & PAD_UP){
                if(sel<=WFONTS*HFONTS){
                    if(sel>=WFONTS) sel-=WFONTS;
                }else{
                    sel-=4;
                }
            }else if(new_pad & PAD_DOWN){
                if(sel/WFONTS == HFONTS-1){
                    if(sel%WFONTS < 5)	sel=WFONTS*HFONTS;
                    else				sel=WFONTS*HFONTS+1;
                }else if(sel/WFONTS <= HFONTS-2)
                    sel+=WFONTS;
            }else if(new_pad & PAD_LEFT){
                if(sel>0) sel--;
            }else if(new_pad & PAD_RIGHT){
                if(sel<=WFONTS*HFONTS) sel++;
            }else if(new_pad & PAD_START){
                sel = WFONTS*HFONTS;
            }else if(new_pad & PAD_L1){
                if(cur>0) cur--;
                t=0;
            }else if(new_pad & PAD_R1){
                if(cur<strlen(out)) cur++;
                t=0;
            }else if((!swapKeys && new_pad & PAD_CROSS)
                  || (swapKeys && new_pad & PAD_CIRCLE) ){
                if(cur>0){
                    strcpy(tmp, out);
                    out[cur-1]=0;
                    strcat(out, &tmp[cur]);
                    cur--;
                    t=0;
                }
            }else if((swapKeys && new_pad & PAD_CROSS)
                  || (!swapKeys && new_pad & PAD_CIRCLE) ){
                i=strlen(out);
                if(sel < WFONTS*HFONTS){  //Any char in matrix selected ?
                    if(i<max && i<33){
                        strcpy(tmp, out);
                        out[cur]=KEY[sel];
                        out[cur+1]=0;
                        strcat(out, &tmp[cur]);
                        cur++;
                        t=0;
                    }
                }else if(sel == WFONTS*HFONTS){ //'OK' exit-button selected ?
                    break;                        //break out of loop with i==strlen
                }else  //Must be 'CANCEL' exit-button
                    return -1;
            }
        }

        //Kbd response section
        if(PS2KbdRead(&KeyPress)) {
            strcpy(tmp, out);
            sprintf(out+cur," %02X %n",KeyPress, &x);
            if(cur+x < strlen(tmp))
                strcat(out, tmp+cur+x);
            cur+=x;
            if(cur>=31)
                cur=0;
            t=0;
        } //ends if(PS2KbdRead(&KeyPress))

        t++;

        if(t & 0x0F) event |= 4;  //repetitive timer event

        if(event||post_event){ //NB: We need to update two frame buffers per event

            //Display section
            drawPopSprite(setting->color[COLOR_BACKGR],
                KEY_X, KEY_Y,
                KEY_X+KEY_W, KEY_Y+KEY_H);
            drawFrame(
                KEY_X, KEY_Y,
                KEY_X+KEY_W, KEY_Y+KEY_H, setting->color[COLOR_FRAME]);
            drawOpSprite(setting->color[COLOR_FRAME],
                KEY_X, KEY_Y+20,
                KEY_X+KEY_W, KEY_Y+20+LINE_THICKNESS);
            printXY(out, KEY_X+2+3, KEY_Y+3, setting->color[COLOR_TEXT], TRUE, 0);
            if(((event|post_event)&4) && (t & 0x10)){
                printXY("|",
                    KEY_X+cur*8+1, KEY_Y+3, setting->color[COLOR_TEXT], TRUE, 0);
            }
            for(i=0; i<KEY_LEN; i++)
                drawChar(KEY[i],
                    KEY_X+2+4 + (i%WFONTS+1)*20 - 12,
                    KEY_Y+28 + (i/WFONTS)*16,
                    setting->color[COLOR_TEXT]);
            printXY("OK                       CANCEL",
                KEY_X+2+4 + 20 - 12, KEY_Y+28 + HFONTS*16, setting->color[COLOR_TEXT], TRUE, 0);

            //Cursor positioning section
            if(sel<=WFONTS*HFONTS)
                x = KEY_X+2+4 + (sel%WFONTS+1)*20 - 20;
            else
                x = KEY_X+2+4 + 25*8;
            y = KEY_Y+28 + (sel/WFONTS)*16;
            drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

            //Tooltip section
            x = SCREEN_MARGIN;
            y = Menu_tooltip_y;
            drawSprite(setting->color[COLOR_BACKGR], 0, y-1, SCREEN_WIDTH, y+FONT_HEIGHT);

            if (swapKeys)
                printXY("\xFF""1:OK \xFF""0:Back L1:Left R1:Right START:Enter",
                    x, y, setting->color[COLOR_SELECT], TRUE, 0);
            else
                printXY("\xFF""0:OK \xFF""1:Back L1:Left R1:Right START:Enter",
                    x, y, setting->color[COLOR_SELECT], TRUE, 0);
        }//ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }//ends while
    return i;
}
*/
//------------------------------
// endfunc keyboard2  (commented out except in testing)
//--------------------------------------------------------------
int setFileList(const char *path, const char *ext, FILEINFO *files, int cnfmode)
{
    int nfiles, i, j, ret;

    size_valid = 0;
    time_valid = 0;

    nfiles = 0;
    if (path[0] == 0) {
        //-- Start case for browser root pseudo folder with device links --
        if (USB_mass_scanned)  // if mass drives were scanned in earlier browsing
            scan_USB_mass();   // then allow another scan here (timer dependent)

        strcpy(files[nfiles].name, "mc0:");
        files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
        strcpy(files[nfiles].name, "mc1:");
        files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
        strcpy(files[nfiles].name, "hdd0:");
        files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
        strcpy(files[nfiles].name, "dvr_hdd0:");
        files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
        strcpy(files[nfiles].name, "cdfs:");
        files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
        if ((cnfmode != USBD_IRX_CNF) && (cnfmode != USBKBD_IRX_CNF) && (cnfmode != USBMASS_IRX_CNF)) {
            // The condition above blocks selecting USB drivers from USB devices
            if (USB_mass_ix[0] || !USB_mass_scanned) {
                strcpy(files[nfiles].name, "mass:");
                files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
            }
            for (i = 1; i < 10; i++) {
                if (USB_mass_ix[i]) {
                    strcpy(files[nfiles].name, "mass0:");
                    files[nfiles].name[4] = USB_mass_ix[i];
                    files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
                }
            }
        }
        if (!cnfmode || (cnfmode == JPG_CNF)) {
            // This condition blocks selecting any CONFIG items on PC
            // or in a virtual memory card
            strcpy(files[nfiles].name, "host:");
            files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
            if (vmcMounted[0]) {
                strcpy(files[nfiles].name, "vmc0:");
                files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
            }
            if (vmcMounted[1]) {
                strcpy(files[nfiles].name, "vmc1:");
                files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
            }
        }
        if (cnfmode < 2) {
            // This condition blocks use of MISC pseudo-device for drivers and skins
            // And allows this device only for launch keys and for normal browsing
            strcpy(files[nfiles].name, LNG(MISC));
            files[nfiles].stats.AttrFile = sceMcFileAttrSubdir;
            nfiles++;
        }
        for (i = 0; i < nfiles; i++)
            files[i].title[0] = 0;
        vfreeSpace = FALSE;
        //-- End case for browser root pseudo folder with device links --
    } else if (!strcmp(path, setting->Misc)) {
        //-- Start case for MISC command pseudo folder with function links --
        nfiles = 0;
        strcpy(files[nfiles].name, "..");
        files[nfiles++].stats.AttrFile = sceMcFileAttrSubdir;
        if (cnfmode) {  // Stop recursive FileBrowser entry, only allow it for launch keys
            strcpy(files[nfiles].name, LNG(FileBrowser));
            files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        }
        strcpy(files[nfiles].name, LNG(PS2Browser));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(PS2Disc));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(PS2Net));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(PS2PowerOff));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(HddManager));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(TextEditor));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(JpgViewer));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(Configure));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(Load_CNFprev));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(Load_CNFnext));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(Set_CNF_Path));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(Load_CNF));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        // Next 2 lines add an optional font test routine
        strcpy(files[nfiles].name, LNG(ShowFont));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(Debug_Info));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(About_uLE));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        strcpy(files[nfiles].name, LNG(OSDSYS));
        files[nfiles++].stats.AttrFile = sceMcFileAttrFile;
        for (i = 0; i < nfiles; i++)
            files[i].title[0] = 0;
        //-- End case for MISC command pseudo folder with function links --
    } else {
        //-- Start case for normal folder with file/folder links --
        strcpy(files[0].name, "..");
        files[0].stats.AttrFile = sceMcFileAttrSubdir;
        nfiles = getDir(path, &files[1]) + 1;
        if (strcmp(ext, "*")) {
            for (i = j = 1; i < nfiles; i++) {
                if (files[i].stats.AttrFile & sceMcFileAttrSubdir)
                    files[j++] = files[i];
                else {
                    if (genCmpFileExt(files[i].name, ext))
                        files[j++] = files[i];
                }
            }
            nfiles = j;
        }
        if ((file_show == 2) || (file_sort == 2)) {
            for (i = 1; i < nfiles; i++) {
                ret = getGameTitle(path, &files[i], files[i].title);
                if (ret < 0)
                    files[i].title[0] = 0;
            }
        }
        if (!strcmp(path, "hdd0:/") || !strcmp(path, "dvr_hdd0:/"))
            vfreeSpace = FALSE;
        else if (nfiles > 1)
            sort(&files[1], 0, nfiles - 2);
        //-- End case for normal folder with file/folder links --
    }
    return nfiles;
}
//------------------------------
// endfunc setFileList
//--------------------------------------------------------------
int BrowserModePopup(void)
{
    char tmp[80];
    int y, i, test;
    int event, post_event = 0;

    int entry_file_show = file_show;
    int entry_file_sort = file_sort;

    int Show_len = strlen(LNG(Show_Content_as)) + 1;
    int Sort_len = strlen(LNG(Sort_Content_by)) + 1;

    int menu_len = Show_len;
    if (menu_len < (i = Sort_len))
        menu_len = i;
    if (menu_len < (i = strlen(LNG(Filename)) + strlen(LNG(_plus_Details))))
        menu_len = i;
    if (menu_len < (i = strlen(LNG(Game_Title)) + strlen(LNG(_plus_Details))))
        menu_len = i;
    if (menu_len < (i = strlen(LNG(No_Sort))))
        menu_len = i;
    if (menu_len < (i = strlen(LNG(Timestamp))))
        menu_len = i;
    if (menu_len < (i = strlen(LNG(Back_to_Browser))))
        menu_len = i;
    menu_len += 3;  // All of the above strings are indented 3 spaces, for tooltips

    int menu_ch_w = menu_len + 1;  // Total characters in longest menu string
    int menu_ch_h = 14;            // Total number of menu lines
    int mSprite_w = (menu_ch_w + 3) * FONT_WIDTH;
    int mSprite_h = (menu_ch_h + 1) * FONT_HEIGHT;
    int mSprite_X1 = SCREEN_WIDTH / 2 - mSprite_w / 2;
    int mSprite_Y1 = SCREEN_HEIGHT / 2 - mSprite_h / 2;
    int mSprite_X2 = mSprite_X1 + mSprite_w;
    int mSprite_Y2 = mSprite_Y1 + mSprite_h;

    char minuses_s[81];

    for (i = 0; i < 80; i++)
        minuses_s[i] = '-';
    minuses_s[80] = '\0';

    event = 1;  // event = initial entry
    while (1) {
        // Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            switch (new_pad) {
                case PAD_RIGHT:
                    file_sort = 0;
                    event |= 2;  // event |= valid pad command
                    break;
                case PAD_DOWN:
                    file_sort = 1;
                    event |= 2;  // event |= valid pad command
                    break;
                case PAD_LEFT:
                    file_sort = 2;
                    event |= 2;  // event |= valid pad command
                    break;
                case PAD_UP:
                    file_sort = 3;
                    event |= 2;  // event |= valid pad command
                    break;
                case PAD_CIRCLE:
                    file_show = 0;
                    event |= 2;  // event |= valid pad command
                    break;
                case PAD_CROSS:
                    file_show = 1;
                    event |= 2;  // event |= valid pad command
                    break;
                case PAD_SQUARE:
                    file_show = 2;
                    event |= 2;  // event |= valid pad command
                    if ((elisaFnt == NULL) && (elisa_failed == FALSE)) {
                        int fd, res;
                        elisa_failed = TRUE;  // Default to FAILED. If it succeeds, then this status will be cleared.

                        res = genFixPath("uLE:/ELISA100.FNT", tmp);
                        if (!strncmp(tmp, "cdrom", 5))
                            strcat(tmp, ";1");
                        if (res >= 0) {
                            fd = genOpen(tmp, O_RDONLY);
                            if (fd >= 0) {
                                test = (int)genLseek(fd, 0, SEEK_END);
                                if (test == 55016) {
                                    elisaFnt = (unsigned char *)memalign(64, test);
                                    genLseek(fd, 0, SEEK_SET);
                                    genRead(fd, elisaFnt, test);

                                    elisa_failed = FALSE;
                                }
                                genClose(fd);
                            }
                        }
                    }
                    break;
                case PAD_TRIANGLE:
                    return (file_show != entry_file_show) || (file_sort != entry_file_sort);
            }  // ends switch(new_pad)
        }      // ends if(readpad())

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            drawPopSprite(setting->color[COLOR_BACKGR],
                          mSprite_X1, mSprite_Y1,
                          mSprite_X2, mSprite_Y2);
            drawFrame(mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, setting->color[COLOR_FRAME]);

            for (i = 0, y = mSprite_Y1 + FONT_HEIGHT / 2; i < menu_ch_h; i++) {
                if (i == 0)
                    sprintf(tmp, "   %s:", LNG(Show_Content_as));
                else if (i == 1)
                    sprintf(tmp, "   %s", &minuses_s[80 - Show_len]);
                else if (i == 2)
                    sprintf(tmp, "\xFF"
                                 "0 %s",
                            LNG(Filename));
                else if (i == 3)
                    sprintf(tmp, "\xFF"
                                 "1 %s%s",
                            LNG(Filename), LNG(_plus_Details));
                else if (i == 4)
                    sprintf(tmp, "\xFF"
                                 "2 %s%s",
                            LNG(Game_Title), LNG(_plus_Details));
                else if (i == 6)
                    sprintf(tmp, "   %s:", LNG(Sort_Content_by));
                else if (i == 7)
                    sprintf(tmp, "   %s", &minuses_s[80 - Sort_len]);
                else if (i == 8)
                    sprintf(tmp, "\xFF"
                                 ": %s",
                            LNG(No_Sort));
                else if (i == 9)
                    sprintf(tmp, "\xFF"
                                 "; %s",
                            LNG(Filename));
                else if (i == 10)
                    sprintf(tmp, "\xFF"
                                 "< %s",
                            LNG(Game_Title));
                else if (i == 11)
                    sprintf(tmp, "\xFF"
                                 "= %s",
                            LNG(Timestamp));
                else if (i == 13)
                    sprintf(tmp, "\xFF"
                                 "3 %s",
                            LNG(Back_to_Browser));
                else
                    tmp[0] = 0;

                printXY(tmp, mSprite_X1 + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
                // Display marker for current modes
                if ((file_show == i - 2) || (file_sort == i - 8))
                    drawChar(LEFT_CUR, mSprite_X1 + FONT_WIDTH / 2, y, setting->color[COLOR_SELECT]);
                y += FONT_HEIGHT;

            }  // ends for loop handling one text row per loop

            // Tooltip section
            //  x = SCREEN_MARGIN;
            y = Menu_tooltip_y;
            drawSprite(setting->color[COLOR_BACKGR],
                       0, y - 1,
                       SCREEN_WIDTH, y + FONT_HEIGHT);
        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while
}
//------------------------------
// endfunc BrowserModePopup
//--------------------------------------------------------------
// get_FilePath is the main browser function.
// It also contains the menu handler for the R1 submenu
// The static variables declared here are only for the use of
// this function and the submenu functions that it calls
//--------------------------------------------------------------
// sincro: ADD USBD_IRX_CNF mode for found IRX file for USBD.IRX
// example: getFilePath(setting->usbd_file, USBD_IRX_CNF);
// polo: ADD SKIN_CNF mode for found jpg file for SKIN
// example: getFilePath(setting->skin, SKIN_CNF);
// suloku: ADD GUI_SKIN_CNF mode for found jpg file for MAIN_SKIN
// example: getFilePath(setting->GUI_skin, GUI_SKIN_CNF);
// dlanor: ADD USBKBD_IRX_CNF mode for found IRX file for USBKBD.IRX
// example: getFilePath(setting->usbkbd_file, USBKBD_IRX_CNF);
// dlanor: ADD USBMASS_IRX_CNF mode for found IRX file for usb_mass
// example: getFilePath(setting->usbmass_file, USBMASS_IRX_CNF);
// dlanor: ADD SAVE_CNF mode returning either pure path or pathname
// dlanor: ADD return value 0=pure path, 1=pathname, negative=error/no_selection
static int browser_cd, browser_up, browser_repos, browser_pushed;
static int browser_sel, browser_nfiles;
static void submenu_func_GetSize(char *mess, char *path, FILEINFO *files);
static void submenu_func_Paste(char *mess, char *path);
static void submenu_func_mcPaste(char *mess, char *path);
static void submenu_func_psuPaste(char *mess, char *path);
int getFilePath(char *out, int cnfmode)
{
    char path[MAX_PATH + 1], cursorEntry[MAX_PATH],
        msg0[MAX_PATH], msg1[MAX_PATH],
        tmp[MAX_PATH], tmp1[MAX_PATH], tmp2[MAX_PATH], ext[8], *p;
    const unsigned char *mcTitle;
    u64 color;
    FILEINFO files[MAX_ENTRY];
    int top = 0, rows;
    int x, y;
    int i, j, ret, rv = -1;  // NB: rv is for return value of this function
    int event, post_event = 0;
    int font_height;
    int iconbase, iconcolr;

    elisa_failed = FALSE;  // set at failure to load font, cleared at each browser entry

    browser_cd = TRUE;
    browser_up = FALSE;
    browser_repos = FALSE;
    browser_pushed = TRUE;
    browser_sel = 0;
    browser_nfiles = 0;

    strcpy(ext, cnfmode_extL[cnfmode]);

    if ((cnfmode == USBD_IRX_CNF) || (cnfmode == USBKBD_IRX_CNF) || (cnfmode == USBMASS_IRX_CNF) || ((!strncmp(LastDir, setting->Misc, strlen(setting->Misc))) && (cnfmode > LK_ELF_CNF)))
        path[0] = '\0';  // start in main root if recent folder unreasonable
    else
        strcpy(path, LastDir);  // If reasonable, start in recent folder

    unmountAll();  // unmount all uLE-used mountpoints

    clipPath[0] = 0;
    nclipFiles = 0;
    browser_cut = 0;

    file_show = 1;
    file_sort = 1;

    font_height = FONT_HEIGHT;
    rows = (Menu_end_y - Menu_start_y) / font_height;

    event = 1;  // event = initial entry
    while (1) {

        // Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad) {
                browser_pushed = TRUE;
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
            else if (new_pad & PAD_TRIANGLE)
                browser_up = TRUE;
            else if ((swapKeys && (new_pad & PAD_CROSS)) || (!swapKeys && (new_pad & PAD_CIRCLE))) {  // Pushed OK
                if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir) {
                    // pushed OK for a folder
                    if (!strcmp(files[browser_sel].name, ".."))
                        browser_up = TRUE;
                    else {
                        strcat(path, files[browser_sel].name);
                        strcat(path, "/");
                        browser_cd = TRUE;
                    }
                } else {
                    // pushed OK for a file
                    sprintf(out, "%s%s", path, files[browser_sel].name);
                    // Must to include a function for check IRX Header
                    if (((cnfmode == LK_ELF_CNF) || (cnfmode == NON_CNF)) && (!IsSupportedFileType(out))) {
                        browser_pushed = FALSE;
                        sprintf(msg0, "%s.", LNG(This_file_isnt_an_ELF));
                        out[0] = 0;
                    } else {
                        strcpy(LastDir, path);
                        rv = 1;  // flag pathname selected
                        break;
                    }
                }
            } else if (new_pad & PAD_R3) {  // New clause for uLE-relative paths
                if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir) {
                    // pushed R3 for a folder (navigate to uLE CNF folder)
                    strcpy(path, LaunchElfDir);
                    if ((p = strchr(path, ':'))) {                         // device separator ?
                        if (p[1] != '/') {                                 // missing path separator ? (mass: & host:)
                            p[1] = '/';                                    // insert path separator
                            strcpy(p + 2, LaunchElfDir + (p - path) + 1);  // append rest of pathname
                        }
                    }
                    browser_cd = TRUE;
                } else {
                    // pushed R3 for a file (treat as uLE-related)
                    sprintf(out, "%s%s", path, files[browser_sel].name);
                    // Must to include a function for check IRX Header
                    if (((cnfmode == LK_ELF_CNF) || (cnfmode == NON_CNF)) && (checkELFheader(out) < 0)) {
                        browser_pushed = FALSE;
                        sprintf(msg0, "%s.", LNG(This_file_isnt_an_ELF));
                        out[0] = 0;
                    } else {
                        strcpy(LastDir, path);
                        sprintf(out, "%s%s", "uLE:/", files[browser_sel].name);
                        rv = 1;  // flag pathname selected
                        break;
                    }
                }
            } else if (new_pad & PAD_R2) {
                char *temp = PathPad_menu(path);

                if (temp != NULL) {
                    strcpy(path, temp);
                    browser_cd = TRUE;
                    vfreeSpace = FALSE;
                }
            } else if (new_pad & PAD_L1) {
                browser_cd = BrowserModePopup();
            }
            // pad checks above are for commands common to all browser modes
            // pad checks below are for commands that differ depending on cnfmode
            if (cnfmode == DIR_CNF) {
                if (new_pad & PAD_START) {
                    strcpy(out, path);
                    strcpy(LastDir, path);
                    rv = 0;  // flag pathname selected
                    break;
                }
            } else if (cnfmode == SAVE_CNF) {  // Generic Save commands
                if (new_pad & PAD_START) {
                    if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir) {
                        // no file was highlighted, so prep to save with empty filename
                        strcpy(out, path);
                        rv = 0;  // flag pure path selected
                    } else {
                        // a file was highlighted, so prep to save with that filename
                        sprintf(out, "%s%s", path, files[browser_sel].name);
                        rv = 1;  // flag pathname selected
                    }
                    strcpy(LastDir, path);
                    break;
                }
            }
            if (cnfmode) {  // A file is to be selected, not in normal browser mode
                if (new_pad & PAD_SQUARE) {
                    if (!strcmp(ext, "*"))
                        strcpy(ext, cnfmode_extL[cnfmode]);
                    else
                        strcpy(ext, "*");
                    browser_cd = TRUE;
                } else if ((!swapKeys && (new_pad & PAD_CROSS)) || (swapKeys && (new_pad & PAD_CIRCLE))) {  // Cancel command ?
                    unmountAll();
                    return rv;
                }
            } else {  // cnfmode == FALSE
                if (new_pad & PAD_R1) {
                    ret = menu(path, &files[browser_sel]);
                    if (ret == COPY || ret == CUT) {
                        strcpy(clipPath, path);
                        if (nmarks > 0) {
                            for (i = nclipFiles = 0; i < browser_nfiles; i++)
                                if (marks[i])
                                    clipFiles[nclipFiles++] = files[i];
                        } else {
                            clipFiles[0] = files[browser_sel];
                            nclipFiles = 1;
                        }
                        sprintf(msg0, "%s", LNG(Copied_to_the_Clipboard));
                        browser_pushed = FALSE;
                        if (ret == CUT)
                            browser_cut = TRUE;
                        else
                            browser_cut = FALSE;
                    }  // ends COPY and CUT
                    else if (ret == DELETE) {
                        if (nmarks == 0) {  // dlanor: using title was inappropriate here (filesystem op)
                            sprintf(tmp, "%s", files[browser_sel].name);
                            if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir)
                                strcat(tmp, "/");
                            sprintf(tmp1, "\n%s ?", LNG(Delete));
                            strcat(tmp, tmp1);
                            ret = ynDialog(tmp);
                        } else
                            ret = ynDialog(LNG(Mark_Files_Delete));

                        if (ret > 0) {
                            int first_deleted = 0;
                            if (nmarks == 0) {
                                strcpy(tmp, files[browser_sel].name);
                                if (files[browser_sel].stats.AttrFile & sceMcFileAttrSubdir)
                                    strcat(tmp, "/");
                                sprintf(tmp1, " %s", LNG(deleting));
                                strcat(tmp, tmp1);
                                drawMsg(tmp);
                                ret = delete (path, &files[browser_sel]);
                            } else {
                                for (i = 0; i < browser_nfiles; i++) {
                                    if (marks[i]) {
                                        if (!first_deleted)     // if this is the first mark
                                            first_deleted = i;  // then memorize it for cursor positioning
                                        strcpy(tmp, files[i].name);
                                        if (files[i].stats.AttrFile & sceMcFileAttrSubdir)
                                            strcat(tmp, "/");
                                        sprintf(tmp1, " %s", LNG(deleting));
                                        strcat(tmp, tmp1);
                                        drawMsg(tmp);
                                        ret = delete (path, &files[i]);
                                        if (ret < 0)
                                            break;
                                    }
                                }
                            }
                            if (ret >= 0) {
                                if (nmarks == 0)
                                    strcpy(cursorEntry, files[browser_sel - 1].name);
                                else
                                    strcpy(cursorEntry, files[first_deleted - 1].name);
                            } else {
                                strcpy(cursorEntry, files[browser_sel].name);
                                sprintf(msg0, "%s Err=%d", LNG(Delete_Failed), ret);
                                browser_pushed = FALSE;
                            }
                            browser_cd = TRUE;
                            browser_repos = TRUE;
                        }
                    }  // ends DELETE
                    else if (ret == RENAME) {
                        strcpy(tmp, files[browser_sel].name);
                        if (keyboard(tmp, 36) > 0) {
                            if (Rename(path, &files[browser_sel], tmp) < 0) {
                                browser_pushed = FALSE;
                                strcpy(msg0, LNG(Rename_Failed));
                            } else
                                browser_cd = TRUE;
                        }
                    }  // ends RENAME
                    else if (ret == PASTE)
                        submenu_func_Paste(msg0, path);
                    else if (ret == MCPASTE)
                        submenu_func_mcPaste(msg0, path);
                    else if (ret == PSUPASTE)
                        submenu_func_psuPaste(msg0, path);
                    else if (ret == NEWDIR) {
                        tmp[0] = 0;
                        if (keyboard(tmp, 36) > 0) {
                            ret = newdir(path, tmp);
                            if (ret == -17) {
                                strcpy(msg0, LNG(directory_already_exists));
                                browser_pushed = FALSE;
                            } else if (ret < 0) {
                                strcpy(msg0, LNG(NewDir_Failed));
                                browser_pushed = FALSE;
                            } else {  // dlanor: modified for similarity to PC browsers
                                sprintf(msg0, "%s: ", LNG(Created_folder));
                                strcat(msg0, tmp);
                                browser_pushed = FALSE;
                                strcpy(cursorEntry, tmp);
                                browser_repos = TRUE;
                                browser_cd = TRUE;
                            }
                        }
                    }  // ends NEWDIR
                    else if (ret == NEWICON) {
                        strcpy(tmp, LNG(Icon_Title));
                        if (keyboard(tmp, 36) <= 0)
                            goto DoneIcon;
                        genFixPath(path, tmp1);
                        strcat(tmp1, "icon.sys");
                        if ((ret = genOpen(tmp1, O_RDONLY)) >= 0) {  // if old "icon.sys" file exists
                            genClose(ret);
                            sprintf(msg1,
                                    "\n\"icon.sys\" %s.\n\n%s ?", LNG(file_alredy_exists),
                                    LNG(Do_you_wish_to_overwrite_it));
                            if (ynDialog(msg1) < 0)
                                goto DoneIcon;
                            genRemove(tmp1);
                        }
                        make_iconsys(tmp, "icon.icn", tmp1);
                        browser_cd = TRUE;
                        strcpy(tmp, LNG(IconText));
                        keyboard(tmp, 36);
                        genFixPath(path, tmp1);
                        strcat(tmp1, "icon.icn");
                        if ((ret = genOpen(tmp1, O_RDONLY)) >= 0) {  // if old "icon.icn" file exists
                            genClose(ret);
                            sprintf(msg1,
                                    "\n\"icon.icn\" %s.\n\n%s ?", LNG(file_alredy_exists),
                                    LNG(Do_you_wish_to_overwrite_it));
                            if (ynDialog(msg1) < 0)
                                goto DoneIcon;
                            genRemove(tmp1);
                        }
                        make_icon(tmp, tmp1);
                    DoneIcon:
                        strcpy(tmp, tmp1);  // Dummy code to make 'goto DoneIcon' legal for gcc
                    }                       // ends NEWICON
                    else if ((ret == MOUNTVMC0) || (ret == MOUNTVMC1)) {
                        i = ret - MOUNTVMC0;
                        load_vmc_fs();
                        sprintf(tmp, "vmc%d:", i);
                        if (vmcMounted[i]) {
                            if ((j = vmc_PartyIndex[i]) >= 0) {
                                vmc_PartyIndex[i] = -1;
                                if (j != vmc_PartyIndex[1 ^ i])
                                    Party_vmcIndex[j] = -1;
                            }
                            fileXioUmount(tmp);
                            vmcMounted[i] = 0;
                        }
                        j = genFixPath(path, tmp1);
                        strcpy(tmp2, tmp1);
                        if (!strncmp(path, "host:", 5)) {
                            makeHostPath(tmp2, tmp1);
                        }
                        strcat(tmp2, files[browser_sel].name);
                        if ((x = fileXioMount(tmp, tmp2, FIO_MT_RDWR)) >= 0) {
                            if ((j >= 0) && (j < MOUNT_LIMIT)) {
                                vmc_PartyIndex[i] = j;
                                Party_vmcIndex[j] = i;
                            }
                            vmcMounted[i] = 1;
                            sprintf(path, "%s/", tmp);
                            browser_cd = TRUE;
                            cnfmode = NON_CNF;
                            strcpy(ext, cnfmode_extL[cnfmode]);
                        } else {
                            sprintf(msg1, "\n'%s vmc%d:' for \"%s\"\nResult=%d",
                                    LNG(Mount), i, tmp2, x);
                            (void)ynDialog(msg1);
                        }
                    }  // ends MOUNTVMCx
                    else if (ret == GETSIZE) {
                        submenu_func_GetSize(msg0, path, files);
                    }
                    // ends GETSIZE
                    // R1 menu handling is completed above
                } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                    if (browser_sel != 0 && path[0] != 0 && (strcmp(path, "hdd0:/") && strcmp(path, "dvr_hdd0:/"))) {
                        if (marks[browser_sel]) {
                            marks[browser_sel] = FALSE;
                            nmarks--;
                        } else {
                            marks[browser_sel] = TRUE;
                            nmarks++;
                        }
                    }
                    browser_sel++;
                } else if (new_pad & PAD_SQUARE) {
                    if (path[0] != 0 && (strcmp(path, "hdd0:/") && strcmp(path, "dvr_hdd0:/"))) {
                        for (i = 1; i < browser_nfiles; i++) {
                            if (marks[i]) {
                                marks[i] = FALSE;
                                nmarks--;
                            } else {
                                marks[i] = TRUE;
                                nmarks++;
                            }
                        }
                    }
                } else if (new_pad & PAD_SELECT) {  // Leaving the browser ?
                    unmountAll();
                    return rv;
                }
            }
        }  // ends pad response section

        // browser path adjustment section
        if (browser_up) {
            if ((p = strrchr(path, '/')) != NULL)
                *p = 0;
            if ((p = strrchr(path, '/')) != NULL) {
                p++;
                strcpy(cursorEntry, p);
                *p = 0;
            } else {
                strcpy(cursorEntry, path);
                path[0] = 0;
            }
            browser_cd = TRUE;
            browser_repos = TRUE;
        }  // ends 'if(browser_up)'
        //----- Process newly entered directory here (incl initial entry)
        if (browser_cd) {
            browser_nfiles = setFileList(path, ext, files, cnfmode);
            if (!cnfmode) {  // Calculate free space (unless configuring)
                if (!strncmp(path, "mc", 2)) {
                    mcGetInfo(path[2] - '0', 0, &mctype_PSx, &mcfreeSpace, NULL);
                    mcSync(0, NULL, &ret);
                    freeSpace = mcfreeSpace * ((mctype_PSx == 1) ? 8192 : 1024);
                    vfreeSpace = TRUE;
                } else if (!strncmp(path, "vmc", 3)) {
                    strncpy(tmp, path, 5);
                    tmp[5] = '\0';
                    freeSpace = fileXioDevctl(tmp, DEVCTL_VMCFS_CKFREE, NULL, 0, NULL, 0);
                    vfreeSpace = TRUE;
                } else if (!strncmp(path, "hdd", 3) && strcmp(path, "hdd0:/")) {
                    u64 ZoneFree, ZoneSize;
                    char pfs_str[6];

                    strcpy(pfs_str, "pfs0:");
                    pfs_str[3] += latestMount;
                    ZoneFree = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_FREE, NULL, 0, NULL, 0);
                    ZoneSize = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_SIZE, NULL, 0, NULL, 0);
                    // printf("ZoneFree==%d  ZoneSize==%d\r\n", ZoneFree, ZoneSize);
                    freeSpace = ZoneFree * ZoneSize;
                    vfreeSpace = TRUE;
                } else if (!strncmp(path, "dvr_hdd", 7) && strcmp(path, "dvr_hdd0:/")) {
                    u64 ZoneFree, ZoneSize;
                    char pfs_str[10];

                    strcpy(pfs_str, "dvr_pfs0:");
                    pfs_str[7] += latestMount;
                    ZoneFree = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_FREE, NULL, 0, NULL, 0);
                    ZoneSize = fileXioDevctl(pfs_str, PFSCTL_GET_ZONE_SIZE, NULL, 0, NULL, 0);
                    // printf("ZoneFree==%d  ZoneSize==%d\r\n", ZoneFree, ZoneSize);
                    freeSpace = ZoneFree * ZoneSize;
                    vfreeSpace = TRUE;
                }
            }
            browser_sel = 0;
            top = 0;
            if (browser_repos) {
                browser_repos = FALSE;
                for (i = 0; i < browser_nfiles; i++) {
                    if (!strcmp(cursorEntry, files[i].name)) {
                        browser_sel = i;
                        top = browser_sel - 3;
                        break;
                    }
                }
            }  // ends if(browser_repos)
            nmarks = 0;
            memset(marks, 0, MAX_ENTRY);
            browser_cd = FALSE;
            browser_up = FALSE;
        }  // ends if(browser_cd)
        if (!strncmp(path, "cdfs", 4))
            uLE_cdStop();
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

        if (event || post_event) {  // NB: We need to update two frame buffers per event

            // Display section
            clrScr(setting->color[COLOR_BACKGR]);

            x = Menu_start_x;
            y = Menu_start_y;
            font_height = FONT_HEIGHT;
            if ((file_show == 2) && (elisaFnt != NULL)) {
                y -= 2;
                font_height = FONT_HEIGHT + 2;
            }
            rows = (Menu_end_y - Menu_start_y) / font_height;

            for (i = 0; i < rows; i++)  // Repeat loop for each browser text row
            {
                mcTitle = NULL;      // Assume that normal file/folder names are to be displayed
                int name_limit = 0;  // Assume that no name length problems exist

                if (top + i >= browser_nfiles)
                    break;
                if (top + i == browser_sel)
                    color = setting->color[COLOR_SELECT];  // Highlight cursor line
                else
                    color = setting->color[COLOR_TEXT];

                if (!strcmp(files[top + i].name, ".."))
                    strcpy(tmp, "..");
                else if ((file_show == 2) && files[top + i].title[0] != 0) {
                    mcTitle = files[top + i].title;
                } else {  // Show normal file/folder names
                    strcpy(tmp, files[top + i].name);
                    if (file_show > 0) {  // Does display mode include file details ?
                        name_limit = 43 * 8;
                    } else {  // Filenames are shown without file details
                        name_limit = 71 * 8;
                    }
                }
                if (name_limit) {                   // Do we need to check name length ?
                    int name_end = name_limit / 7;  // Max string length for acceptable spacing

                    if (files[top + i].stats.AttrFile & sceMcFileAttrSubdir)
                        name_end -= 1;             // For folders, reserve one character for final '/'
                    if (strlen(tmp) > name_end) {  // Is name too long for clean display ?
                        tmp[name_end - 1] = '~';   // indicate filename abbreviation
                        tmp[name_end] = 0;         // abbreviate name length to make room for details
                    }
                }

                if (files[top + i].stats.AttrFile & sceMcFileAttrSubdir)
                    strcat(tmp, "/");
                if (mcTitle != NULL)
                    printXY_sjis(mcTitle, x + 4, y, color, TRUE);
                else
                    printXY(tmp, x + 4, y, color, TRUE, name_limit);
                if (file_show > 0) {
                    //					unsigned int size = files[top+i].stats.fileSizeByte;
                    unsigned long long size = ((unsigned long long)files[top + i].stats.Reserve2 << 32) | files[top + i].stats.FileSizeByte;
                    int scale = 0;  // 0==Bytes, 1==KBytes, 2==MBytes, 3==GB
                    char scale_s[6] = " KMGTP";
                    PS2TIME timestamp = *(PS2TIME *)&files[top + i].stats._Modify;

                    if (!size_valid)
                        size = 0;
                    if (!time_valid)
                        memset((void *)&timestamp, 0, sizeof(timestamp));

                    if (!size_valid || !(top + i))
                        strcpy(tmp, "----- B");
                    else {
                        while (size > 99999) {
                            scale++;
                            size /= 1024;
                        }
                        //						sprintf(tmp, "%5u%cB", size, scale_s[scale]);
                        // size shouldn't be over 99999, and seems sprintf doesn't support unsigned long long (%llu crashes)
                        sprintf(tmp, "%5u%cB", (unsigned)size, scale_s[scale]);
                    }

                    if (!time_valid || !(top + i))
                        strcat(tmp, " ----.--.-- --:--:--");
                    else {
                        sprintf(tmp + strlen(tmp), " %04d.%02d.%02d %02d:%02d:%02d",
                                ((timestamp.year < 2256) ? timestamp.year : (timestamp.year - 256)),
                                timestamp.month,
                                timestamp.day,
                                timestamp.hour,
                                timestamp.min,
                                timestamp.sec);
                    }

                    printXY(tmp, x + 4 + 44 * FONT_WIDTH, y, color, TRUE, 0);
                }
                if (setting->FB_NoIcons) {  // if FileBrowser should not use icons
                    if (marks[top + i])
                        drawChar('*', x - 6, y, setting->color[COLOR_TEXT]);
                } else {  // if Icons must be used in front of file/folder names
                    if (files[top + i].stats.AttrFile & sceMcFileAttrSubdir) {
                        iconbase = ICON_FOLDER;
                        iconcolr = COLOR_GRAPH1;
                    } else {
                        iconbase = ICON_FILE;
                        if (genCmpFileExt(files[top + i].name, "ELF"))
                            iconcolr = COLOR_GRAPH2;
                        else if (genCmpFileExt(files[top + i].name, "TXT") || genCmpFileExt(files[top + i].name, "JPG") || genCmpFileExt(files[top + i].name, "JPEG"))
                            iconcolr = COLOR_GRAPH4;
                        else
                            iconcolr = COLOR_GRAPH3;
                    }
                    if (marks[top + i])
                        iconbase += 2;
                    drawChar(iconbase, x - 3 - FONT_WIDTH, y, setting->color[iconcolr]);
                    drawChar(iconbase + 1, x - 3, y, setting->color[iconcolr]);
                }
                y += font_height;
            }                             // ends for, so all browser rows were fixed above
            if (browser_nfiles > rows) {  // if more files than available rows, use scrollbar
                drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
                          SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[COLOR_FRAME]);
                int y0 = (Menu_end_y - Menu_start_y + 8) * ((double)top / browser_nfiles);
                int y1 = (Menu_end_y - Menu_start_y + 8) * ((double)(top + rows) / browser_nfiles);
                drawOpSprite(setting->color[COLOR_FRAME],
                             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, (y0 + Menu_start_y - 4),
                             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, (y1 + Menu_start_y - 4));
            }                  // ends clause for scrollbar
            if (nclipFiles) {  // if Something in clipboard, emulate LED indicator
                u64 LED_colour, RIM_colour = GS_SETREG_RGBA(0, 0, 0, 0);
                int RIM_w = 4, LED_w = 6, indicator_w = LED_w + 2 * RIM_w;
                int x2 = SCREEN_WIDTH - SCREEN_MARGIN - 2, x1 = x2 - indicator_w;
                int y1 = Frame_start_y + 2, y2 = y1 + indicator_w;

                if (browser_cut)
                    LED_colour = GS_SETREG_RGBA(0xC0, 0, 0, 0);  // Red LED == CUT
                else
                    LED_colour = GS_SETREG_RGBA(0, 0xC0, 0, 0);  // Green LED == COPY
                drawOpSprite(RIM_colour, x1, y1, x2, y2);
                drawOpSprite(LED_colour, x1 + RIM_w, y1 + RIM_w, x2 - RIM_w, y2 - RIM_w);
            }  // ends clause for clipboard indicator
            if (browser_pushed)
                sprintf(msg0, "%s: %s", LNG(Path), path);

            // Tooltip section
            if (cnfmode) {  // cnfmode indicates configurable file selection
                if (swapKeys)
                    sprintf(msg1, "\xFF"
                                  "1:%s \xFF"
                                  "0:%s \xFF"
                                  "3:%s \xFF"
                                  "2:",
                            LNG(OK), LNG(Cancel), LNG(Up));
                else
                    sprintf(msg1, "\xFF"
                                  "0:%s \xFF"
                                  "1:%s \xFF"
                                  "3:%s \xFF"
                                  "2:",
                            LNG(OK), LNG(Cancel), LNG(Up));
                if (ext[0] == '*')
                    strcat(msg1, "*->");
                strcat(msg1, cnfmode_extU[cnfmode]);
                if (ext[0] != '*')
                    strcat(msg1, "->*");
                sprintf(tmp, " R2:%s", LNG(PathPad));
                strcat(msg1, tmp);
                if ((cnfmode == DIR_CNF) || (cnfmode == SAVE_CNF)) {  // modes using Start
                    sprintf(tmp, " Start:%s", LNG(Choose));
                    strcat(msg1, tmp);
                }
            } else {  // cnfmode == FALSE
                if (swapKeys)
                    sprintf(msg1, "\xFF"
                                  "1:%s \xFF"
                                  "3:%s \xFF"
                                  "0:%s \xFF"
                                  "2:%s L1:%s R1:%s R2:%s Sel:%s",
                            LNG(OK), LNG(Up), LNG(Mark), LNG(RevMark),
                            LNG(Mode), LNG(Menu), LNG(PathPad), LNG(Exit));
                else
                    sprintf(msg1, "\xFF"
                                  "0:%s \xFF"
                                  "3:%s \xFF"
                                  "1:%s \xFF"
                                  "2:%s L1:%s R1:%s R2:%s Sel:%s",
                            LNG(OK), LNG(Up), LNG(Mark), LNG(RevMark),
                            LNG(Mode), LNG(Menu), LNG(PathPad), LNG(Exit));
            }
            setScrTmp(msg0, msg1);
            if (vfreeSpace) {
                if (freeSpace >= 1024 * 1024)
                    sprintf(tmp, "[%.1fMB %s]", (double)freeSpace / 1024 / 1024, LNG(free));
                else if (freeSpace >= 1024)
                    sprintf(tmp, "[%.1fKB %s]", (double)freeSpace / 1024, LNG(free));
                else
                    sprintf(tmp, "[%dB %s]", (int)freeSpace, LNG(free));
                ret = strlen(tmp);
                drawSprite(setting->color[COLOR_BACKGR],
                           SCREEN_WIDTH - SCREEN_MARGIN - (ret + 1) * FONT_WIDTH, (Menu_message_y - 1),
                           SCREEN_WIDTH - SCREEN_MARGIN, (Menu_message_y + FONT_HEIGHT));
                printXY(tmp,
                        SCREEN_WIDTH - SCREEN_MARGIN - ret * FONT_WIDTH,
                        (Menu_message_y),
                        setting->color[COLOR_SELECT], TRUE, 0);
            }
        }  // ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  // ends while

    // Leaving the browser
    unmountAll();
    return rv;
}
//------------------------------
// endfunc getFilePath
//--------------------------------------------------------------
void submenu_func_GetSize(char *mess, char *path, FILEINFO *files)
{
    u64 size;
    int ret, i, text_pos, text_inc, sel = -1;
    char filepath[MAX_PATH];

    /*
    int test;
    iox_stat_t stats;
    PS2TIME *time;
*/

    drawMsg(LNG(Checking_Size));
    if (nmarks == 0) {
        size = getFileSize(path, &files[browser_sel]);
        sel = browser_sel;  // for stat checking
    } else {
        for (i = size = 0; i < browser_nfiles; i++) {
            if (marks[i]) {
                size += getFileSize(path, &files[i]);
                sel = i;  // for stat checking
            }
            if (size < 0)
                size = -1;
        }
    }
    printf("size result = %llu\r\n", size);
    if (size < 0) {
        strcpy(mess, LNG(Size_test_Failed));
        text_pos = strlen(mess);
    } else {
        text_pos = 0;
        if (size >= 1024 * 1024)
            sprintf(mess, "%s = %.1fMB%n", LNG(SIZE), (double)size / 1024 / 1024, &text_inc);
        else if (size >= 1024)
            sprintf(mess, "%s = %.1fKB%n", LNG(SIZE), (double)size / 1024, &text_inc);
        else
            sprintf(mess, "%s = %lluB%n", LNG(SIZE), size, &text_inc);
        text_pos += text_inc;
    }

    //----- Comment out this section to skip attributes entirely -----
    if ((nmarks < 2) && (sel >= 0)) {
        sprintf(filepath, "%s%s", path, files[sel].name);
        //----- Start of section for debug display of attributes -----
        /*
        printf("path =\"%s\"\r\n", path);
        printf("file =\"%s\"\r\n", files[sel].name);
        if	(!strncmp(filepath, "host:/", 6))
            makeHostPath(filepath+5, filepath+6);
        test = fileXioGetStat(filepath, &stats);
        printf("test = %d\r\n", test);
        printf("mode = %08X\r\n", stats.mode);
        printf("attr = %08X\r\n", stats.attr);
        printf("size = %08X\r\n", stats.size);
        time = (PS2TIME *) stats.ctime;
        printf("ctime = %04d.%02d.%02d %02d:%02d:%02d.%02d\r\n",
            time->year,time->month,time->day,
            time->hour,time->min,time->sec,time->unknown);
        time = (PS2TIME *) stats.atime;
        printf("atime = %04d.%02d.%02d %02d:%02d:%02d.%02d\r\n",
            time->year,time->month,time->day,
            time->hour,time->min,time->sec,time->unknown);
        time = (PS2TIME *) stats.mtime;
        printf("mtime = %04d.%02d.%02d %02d:%02d:%02d.%02d\r\n",
            time->year,time->month,time->day,
            time->hour,time->min,time->sec,time->unknown);
*/
        //----- End of section for debug display of attributes -----
        sprintf(mess + text_pos, " m=%04X %04d.%02d.%02d %02d:%02d:%02d%n",
                files[sel].stats.AttrFile,
                files[sel].stats._Modify.Year,
                files[sel].stats._Modify.Month,
                files[sel].stats._Modify.Day,
                files[sel].stats._Modify.Hour,
                files[sel].stats._Modify.Min,
                files[sel].stats._Modify.Sec,
                &text_inc);
        text_pos += text_inc;
        if (!strncmp(path, "mc", 2)) {
            mcGetInfo(path[2] - '0', 0, &mctype_PSx, NULL, NULL);
            mcSync(0, NULL, &ret);
            sprintf(mess + text_pos, " %s=%d%n", LNG(mctype), mctype_PSx, &text_inc);
            text_pos += text_inc;
        }
        // sprintf(mess+text_pos, " mcTsz=%d%n", files[sel].stats.fileSizeByte, &text_inc);
        size = ((unsigned long long)files[sel].stats.Reserve2 << 32) | files[sel].stats.FileSizeByte;
        // Max length is 20 characters+NULL
        char sizeC[21] = {0};
        char *sizeP = &sizeC[21];
        do {
            *(--sizeP) = '0' + (size % 10);
        } while (size /= 10);
        sprintf(mess + text_pos, " mcTsz=%s%n", sizeP, &text_inc);
        text_pos += text_inc;
    }
    //----- End of sections that show attributes -----
    browser_pushed = FALSE;
}
//------------------------------
// endfunc submenu_func_GetSize
//--------------------------------------------------------------
void subfunc_Paste(char *mess, char *path)
{
    char tmp[MAX_PATH], tmp1[MAX_PATH];
    int i, ret = -1;

    written_size = 0;
    PasteTime = Timer();  // Note initial pasting time
    drawMsg(LNG(Pasting));
    if (!strcmp(path, clipPath))
        goto finished;

    for (i = 0; i < nclipFiles; i++) {
        strcpy(tmp, clipFiles[i].name);
        if (clipFiles[i].stats.AttrFile & sceMcFileAttrSubdir)
            strcat(tmp, "/");
        sprintf(tmp1, " %s", LNG(pasting));
        strcat(tmp, tmp1);
        drawMsg(tmp);
        PM_flag[0] = PM_NORMAL;  // Always use normal mode at top level
        PM_file[0] = -1;         // Thus no attribute file is used here
        ret = copy(path, clipPath, clipFiles[i], 0);
        if (ret < 0)
            break;
        if (browser_cut) {
            ret = delete (clipPath, &clipFiles[i]);
            if (ret < 0)
                break;
        }
    }

    //	unmountAll(); //disabled to avoid interference with VMC implementation

finished:
    if (ret < 0) {
        strcpy(mess, LNG(Paste_Failed));
        browser_pushed = FALSE;
    } else if (browser_cut)
        nclipFiles = 0;
    browser_cd = TRUE;
}
//------------------------------
// endfunc subfunc_Paste
//--------------------------------------------------------------
void submenu_func_Paste(char *mess, char *path)
{
    if (new_pad & PAD_SQUARE)
        PasteMode = PM_RENAME;
    else
        PasteMode = PM_NORMAL;
    subfunc_Paste(mess, path);
}
//------------------------------
// endfunc submenu_func_Paste
//--------------------------------------------------------------
void submenu_func_mcPaste(char *mess, char *path)
{
    if (!strncmp(path, "mc", 2) || !strncmp(path, "vmc", 3)) {
        PasteMode = PM_MC_RESTORE;
    } else {
        PasteMode = PM_MC_BACKUP;
    }
    subfunc_Paste(mess, path);
}
//------------------------------
// endfunc submenu_func_mcPaste
//--------------------------------------------------------------
void submenu_func_psuPaste(char *mess, char *path)
{
    if (!strncmp(path, "mc", 2) || !strncmp(path, "vmc", 3)) {
        PasteMode = PM_PSU_RESTORE;
    } else {
        PasteMode = PM_PSU_BACKUP;
    }
    subfunc_Paste(mess, path);
}
//------------------------------
// endfunc submenu_func_psuPaste
//--------------------------------------------------------------
// End of file: filer.c
//--------------------------------------------------------------
