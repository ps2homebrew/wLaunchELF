//---------------------------------------------------------------------------
// File name:   config.c
//---------------------------------------------------------------------------
#include "launchelf.h"
#include <stdbool.h>

enum {
    DEF_TIMEOUT = 10,
    DEF_HIDE_PATHS = TRUE,
    DEF_COLOR1 = GS_SETREG_RGBA(128, 128, 128, 0),  //Backgr
    DEF_COLOR2 = GS_SETREG_RGBA(64, 64, 64, 0),     //Frame
    DEF_COLOR3 = GS_SETREG_RGBA(96, 0, 0, 0),       //Select
    DEF_COLOR4 = GS_SETREG_RGBA(0, 0, 0, 0),        //Text
    DEF_COLOR5 = GS_SETREG_RGBA(96, 96, 0, 0),      //Graph1
    DEF_COLOR6 = GS_SETREG_RGBA(0, 96, 0, 0),       //Graph2
    DEF_COLOR7 = GS_SETREG_RGBA(224, 224, 224, 0),  //Graph3
    DEF_COLOR8 = GS_SETREG_RGBA(0, 0, 0, 0),        //Graph4
    DEF_MENU_FRAME = TRUE,
    DEF_MENU = TRUE,
    DEF_NUMCNF = 1,
    DEF_SWAPKEYS = FALSE,
    DEF_HOSTWRITE = FALSE,
    DEF_BRIGHT = 50,
    DEF_POPUP_OPAQUE = FALSE,
    DEF_INIT_DELAY = 0,
    DEF_USBKBD_USED = 1,
    DEF_SHOW_TITLES = 1,
    DEF_PATHPAD_LOCK = 0,
    DEF_JPGVIEW_TIMER = 5,
    DEF_JPGVIEW_TRANS = 2,
    DEF_JPGVIEW_FULL = 0,
    DEF_PSU_HUGENAMES = 0,
    DEF_PSU_DATENAMES = 0,
    DEF_PSU_NOOVERWRITE = 0,
    DEF_FB_NOICONS = 0,

    DEFAULT = 0,
    SHOW_TITLES = 12,
    FILENAME,
    SCREEN,
    SETTINGS,
    NETWORK,
    OK,
    CANCEL
};

char LK_ID[17][10] = {
    "auto",
    "Circle",
    "Cross",
    "Square",
    "Triangle",
    "L1",
    "R1",
    "L2",
    "R2",
    "L3",
    "R3",
    "Start",
    "Select",  //Predefined for "CONFIG"
    "Left",    //Predefined for "LOAD CONFIG--"
    "Right",   //Predefined for "LOAD CONFIG++"
    "ESR",
    "OSDSYS"};

char PathPad[30][MAX_PATH];
char tmp[MAX_PATH];
SETTING *setting = NULL;
SETTING *tmpsetting;
//---------------------------------------------------------------------------
// End of declarations
// Start of functions
//---------------------------------------------------------------------------
// get_CNF_string is the main CNF parser called for each CNF variable in a
// CNF file. Input and output data is handled via its pointer parameters.
// The return value flags 'false' when no variable is found. (normal at EOF)
//---------------------------------------------------------------------------
int get_CNF_string(char **CNF_p_p,
                   char **name_p_p,
                   char **value_p_p)
{
    char *np, *vp, *tp = *CNF_p_p;

start_line:
    while ((*tp <= ' ') && (*tp > '\0'))
        tp += 1;  //Skip leading whitespace, if any
    if (*tp == '\0')
        return false;  //but exit at EOF
    np = tp;           //Current pos is potential name
    if (*tp < 'A')     //but may be a comment line
    {                  //We must skip a comment line
        while ((*tp != '\r') && (*tp != '\n') && (*tp > '\0'))
            tp += 1;      //Seek line end
        goto start_line;  //Go back to try next line
    }

    while ((*tp >= 'A') || ((*tp >= '0') && (*tp <= '9')))
        tp += 1;  //Seek name end
    if (*tp == '\0')
        return false;  //but exit at EOF

    while ((*tp <= ' ') && (*tp > '\0'))
        *tp++ = '\0';  //zero&skip post-name whitespace
    if (*tp != '=')
        return false;  //exit (syntax error) if '=' missing
    *tp++ = '\0';      //zero '=' (possibly terminating name)

    while ((*tp <= ' ') && (*tp > '\0')  //Skip pre-value whitespace, if any
           && (*tp != '\r') && (*tp != '\n'))
        tp += 1;  //but do not pass the end of the line
    if (*tp == '\0')
        return false;  //but exit at EOF
    vp = tp;           //Current pos is potential value

    while ((*tp != '\r') && (*tp != '\n') && (*tp != '\0'))
        tp += 1;  //Seek line end
    if (*tp != '\0')
        *tp++ = '\0';  //terminate value (passing if not EOF)
    while ((*tp <= ' ') && (*tp > '\0'))
        tp += 1;  //Skip following whitespace, if any

    *CNF_p_p = tp;    //return new CNF file position
    *name_p_p = np;   //return found variable name
    *value_p_p = vp;  //return found variable value
    return true;      //return control to caller
}  //Ends get_CNF_string

//---------------------------------------------------------------------------
int CheckMC(void)
{
    int dummy, ret;

    mcGetInfo(0, 0, &dummy, &dummy, &dummy);
    mcSync(0, NULL, &ret);

    if (-1 == ret || 0 == ret)
        return 0;

    mcGetInfo(1, 0, &dummy, &dummy, &dummy);
    mcSync(0, NULL, &ret);

    if (-1 == ret || 0 == ret)
        return 1;

    return -11;
}
//---------------------------------------------------------------------------
unsigned long hextoul(char *string)
{
    unsigned long value;
    char c;

    value = 0;
    while (!(((c = *string++) < '0') || (c > 'F') || ((c > '9') && (c < 'A'))))
        value = value * 16 + ((c > '9') ? (c - 'A' + 10) : (c - '0'));
    return value;
}
//---------------------------------------------------------------------------
//storeSkinCNF will save most cosmetic settings to a RAM area
//------------------------------
size_t storeSkinCNF(char *cnf_buf)
{
    size_t CNF_size;

    sprintf(cnf_buf,
            "GUI_Col_1_ABGR = %08X\r\n"
            "GUI_Col_2_ABGR = %08X\r\n"
            "GUI_Col_3_ABGR = %08X\r\n"
            "GUI_Col_4_ABGR = %08X\r\n"
            "GUI_Col_5_ABGR = %08X\r\n"
            "GUI_Col_6_ABGR = %08X\r\n"
            "GUI_Col_7_ABGR = %08X\r\n"
            "GUI_Col_8_ABGR = %08X\r\n"
            "SKIN_FILE = %s\r\n"
            "GUI_SKIN_FILE = %s\r\n"
            "SKIN_Brightness = %d\r\n"
            "TV_mode = %d\r\n"
            "Screen_Offset_X = %d\r\n"
            "Screen_Offset_Y = %d\r\n"
            "Popup_Opaque = %d\r\n"
            "Menu_Frame = %d\r\n"
            "Show_Menu = %d\r\n"
            "%n",                    // %n causes NO output, but only a measurement
            (u32)setting->color[COLOR_BACKGR],  //Col_1
            (u32)setting->color[COLOR_FRAME],   //Col_2
            (u32)setting->color[COLOR_SELECT],  //Col_3
            (u32)setting->color[COLOR_TEXT],    //Col_4
            (u32)setting->color[COLOR_GRAPH1],  //Col_5
            (u32)setting->color[COLOR_GRAPH2],  //Col_6
            (u32)setting->color[COLOR_GRAPH3],  //Col_7
            (u32)setting->color[COLOR_GRAPH4],  //Col_8
            setting->skin,           //SKIN_FILE
            setting->GUI_skin,       //GUI_SKIN_FILE
            setting->Brightness,     //SKIN_Brightness
            setting->TV_mode,        //TV_mode
            setting->screen_x,       //Screen_X
            setting->screen_y,       //Screen_Y
            setting->Popup_Opaque,   //Popup_Opaque
            setting->Menu_Frame,     //Menu_Frame
            setting->Show_Menu,      //Show_Menu
            &CNF_size                // This variable measures the size of sprintf data
            );
    return CNF_size;
}
//------------------------------
//endfunc storeSkinCNF
//---------------------------------------------------------------------------
//saveSkinCNF will save most cosmetic settings to a skin CNF file
//------------------------------
int saveSkinCNF(char *CNF)
{
    int ret, fd;
    char tmp[26 * MAX_PATH + 30 * MAX_PATH];
    char cnf_path[MAX_PATH];
    size_t CNF_size;

    CNF_size = storeSkinCNF(tmp);

    ret = genFixPath(CNF, cnf_path);
    if ((ret < 0) || ((fd = genOpen(cnf_path, O_CREAT | O_WRONLY | O_TRUNC)) < 0)) {
        return -1;  //Failed open
    }
    ret = genWrite(fd, &tmp, CNF_size);
    if (ret != CNF_size)
        ret = -2;  //Failed writing
    genClose(fd);

    return ret;
}
//-----------------------------
//endfunc saveSkinCNF
//---------------------------------------------------------------------------
//saveSkinBrowser will save most cosmetic settings to browsed skin CNF file
//------------------------------
void saveSkinBrowser(void)
{
    int tst;
    char path[MAX_PATH];
    char mess[MAX_PATH];
    char *p;

    tst = getFilePath(path, SAVE_CNF);
    if (path[0] == '\0')
        goto abort;
    if (!strncmp(path, "cdfs", 4))
        goto abort;

    drawMsg(LNG(Enter_File_Name));

    tmp[0] = 0;
    if (tst > 0) {               //if an existing file was selected, use its name
        p = strrchr(path, '/');  //find separator between path and name
        if (p != NULL) {
            strcpy(tmp, p + 1);
            p[1] = '\0';
        } else  //if we got a pathname without separator, something is wrong
            goto abort;
    }
    if ((tst >= 0) && (keyboard(tmp, 36) > 0))
        strcat(path, tmp);
    else {
    abort:
        tst = -3;
        goto test;
    }

    tst = saveSkinCNF(path);

test:
    switch (tst) {
        case -1:
            sprintf(mess, "%s \"%s\".", LNG(Failed_To_Save), path);
            break;
        case -2:
            sprintf(mess, "%s \"%s\".", LNG(Failed_writing), path);
            break;
        case -3:
            sprintf(mess, "%s \"%s\".", LNG(Failed_Saving_File), path);
            break;
        default:
            sprintf(mess, "%s \"%s\".", LNG(Saved), path);
    }
    drawMsg(mess);
}
//-----------------------------
//endfunc saveSkinBrowser
//---------------------------------------------------------------------------
//preloadCNF loads an entire CNF file into RAM it allocates
//------------------------------
char *preloadCNF(char *path)
{
    int fd, tst;
    size_t CNF_size;
    char cnf_path[MAX_PATH];
    char *RAM_p;

    fd = -1;
    if ((tst = genFixPath(path, cnf_path)) >= 0)
        fd = genOpen(cnf_path, O_RDONLY);
    if (fd < 0) {
    failed_load:
        return NULL;
    }
    CNF_size = genLseek(fd, 0, SEEK_END);
    printf("CNF_size=%d\n", CNF_size);
    genLseek(fd, 0, SEEK_SET);
    RAM_p = (char *)malloc(CNF_size);
    if (RAM_p == NULL) {
        genClose(fd);
        goto failed_load;
    }
    genRead(fd, RAM_p, CNF_size);  //Read CNF as one long string
    genClose(fd);
    RAM_p[CNF_size] = '\0';  //Terminate the CNF string
    return RAM_p;
}
//------------------------------
//endfunc preloadCNF
//---------------------------------------------------------------------------
//scanSkinCNF will check for most cosmetic variables of a CNF
//------------------------------
int scanSkinCNF(char *name, char *value)
{
    if (!strcmp(name, "GUI_Col_1_ABGR"))
        setting->color[COLOR_BACKGR] = hextoul(value);
    else if (!strcmp(name, "GUI_Col_2_ABGR"))
        setting->color[COLOR_FRAME] = hextoul(value);
    else if (!strcmp(name, "GUI_Col_3_ABGR"))
        setting->color[COLOR_SELECT] = hextoul(value);
    else if (!strcmp(name, "GUI_Col_4_ABGR"))
        setting->color[COLOR_TEXT] = hextoul(value);
    else if (!strcmp(name, "GUI_Col_5_ABGR"))
        setting->color[COLOR_GRAPH1] = hextoul(value);
    else if (!strcmp(name, "GUI_Col_6_ABGR"))
        setting->color[COLOR_GRAPH2] = hextoul(value);
    else if (!strcmp(name, "GUI_Col_7_ABGR"))
        setting->color[COLOR_GRAPH3] = hextoul(value);
    else if (!strcmp(name, "GUI_Col_8_ABGR"))
        setting->color[COLOR_GRAPH4] = hextoul(value);
    //----------
    else if (!strcmp(name, "SKIN_FILE"))
        strcpy(setting->skin, value);
    else if (!strcmp(name, "GUI_SKIN_FILE"))
        strcpy(setting->GUI_skin, value);
    else if (!strcmp(name, "SKIN_Brightness"))
        setting->Brightness = atoi(value);
    //----------
    else if (!strcmp(name, "TV_mode"))
        setting->TV_mode = atoi(value);
    else if (!strcmp(name, "Screen_Offset_X"))
        setting->screen_x = atoi(value);
    else if (!strcmp(name, "Screen_Offset_Y"))
        setting->screen_y = atoi(value);
    //----------
    else if (!strcmp(name, "Popup_Opaque"))
        setting->Popup_Opaque = atoi(value);
    else if (!strcmp(name, "Menu_Frame"))
        setting->Menu_Frame = atoi(value);
    else if (!strcmp(name, "Show_Menu"))
        setting->Show_Menu = atoi(value);
    else
        return 0;  //when no skin variable
    return 1;      //when skin variable found
}
//------------------------------
//endfunc scanSkinCNF
//---------------------------------------------------------------------------
//loadSkinCNF will load most cosmetic settings from CNF file
//------------------------------
int loadSkinCNF(char *path)
{
    int var_cnt;
    char *RAM_p, *CNF_p, *name, *value;

    if (!(RAM_p = preloadCNF(path)))
        return -1;
    CNF_p = RAM_p;
    for (var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++)
        scanSkinCNF(name, value);
    free(RAM_p);
    updateScreenMode();
    if (setting->skin)
        loadSkin(BACKGROUND_PIC, 0, 0);
    return 0;
}
//------------------------------
//endfunc loadSkinCNF
//---------------------------------------------------------------------------
//loadSkinBrowser will load most cosmetic settings from browsed skin CNF file
//------------------------------
void loadSkinBrowser(void)
{
    int tst;
    char path[MAX_PATH];
    char mess[MAX_PATH];

    getFilePath(path, TEXT_CNF);  // No Filtering, Be Careful.
    tst = loadSkinCNF(path);
    if (tst < 0)
        sprintf(mess, "%s \"%s\".", LNG(Failed_To_Load), path);
    else
        sprintf(mess, "%s \"%s\".", LNG(Loaded_Config), path);

    drawMsg(mess);
}
//------------------------------
//endfunc loadSkinBrowser
//---------------------------------------------------------------------------
// Save LAUNCHELF.CNF (or LAUNCHELFx.CNF with multiple pages)
// sincro: ADD save USBD_FILE string
// polo: ADD save SKIN_FILE string
// suloku: ADD save MAIN_SKIN string //dlanor: changed to GUI_SKIN_FILE
//---------------------------------------------------------------------------
void saveConfig(char *mainMsg, char *CNF)
{
    int i, ret, fd;
    char c[MAX_PATH], tmp[26 * MAX_PATH + 30 * MAX_PATH];
    char cnf_path[MAX_PATH];
    size_t CNF_size, CNF_step;

    sprintf(tmp, "CNF_version = 3\r\n%n", &CNF_size);  //Start CNF with version header

    for (i = 0; i < 17; i++) {  //Loop to save the ELF paths for launch keys
        if ((i < 12) || (setting->LK_Flag[i] != 0)) {
            sprintf(tmp + CNF_size,
                    "LK_%s_E1 = %s\r\n"
                    "%n",  // %n causes NO output, but only a measurement
                    LK_ID[i],
                    setting->LK_Path[i],
                    &CNF_step  // This variable measures the size of sprintf data
                    );
            CNF_size += CNF_step;
        }
    }  //ends for

    i = strlen(setting->Misc);
    sprintf(tmp + CNF_size,
            "Misc = %s\r\n"
            "Misc_PS2Disc = %s\r\n"
            "Misc_FileBrowser = %s\r\n"
            "Misc_PS2Browser = %s\r\n"
            "Misc_PS2Net = %s\r\n"
            "Misc_PS2PowerOff = %s\r\n"
            "Misc_HddManager = %s\r\n"
            "Misc_TextEditor = %s\r\n"
            "Misc_JpgViewer = %s\r\n"
            "Misc_Configure = %s\r\n"
            "Misc_Load_CNFprev = %s\r\n"
            "Misc_Load_CNFnext = %s\r\n"
            "Misc_Set_CNF_Path = %s\r\n"
            "Misc_Load_CNF = %s\r\n"
            "Misc_ShowFont = %s\r\n"
            "Misc_Debug_Info = %s\r\n"
            "Misc_About_uLE = %s\r\n"
            "Misc_OSDSYS = %s\r\n"
            "%n",  // %n causes NO output, but only a measurement
            setting->Misc,
            setting->Misc_PS2Disc + i,
            setting->Misc_FileBrowser + i,
            setting->Misc_PS2Browser + i,
            setting->Misc_PS2Net + i,
            setting->Misc_PS2PowerOff + i,
            setting->Misc_HddManager + i,
            setting->Misc_TextEditor + i,
            setting->Misc_JpgViewer + i,
            setting->Misc_Configure + i,
            setting->Misc_Load_CNFprev + i,
            setting->Misc_Load_CNFnext + i,
            setting->Misc_Set_CNF_Path + i,
            setting->Misc_Load_CNF + i,
            setting->Misc_ShowFont + i,
            setting->Misc_Debug_Info + i,
            setting->Misc_About_uLE + i,
            setting->Misc_OSDSYS + i,
            &CNF_step  // This variable measures the size of sprintf data
            );
    CNF_size += CNF_step;

    CNF_size += storeSkinCNF(tmp + CNF_size);

    sprintf(tmp + CNF_size,
            "LK_auto_Timer = %d\r\n"
            "Menu_Hide_Paths = %d\r\n"
            "Menu_Pages = %d\r\n"
            "GUI_Swap_Keys = %d\r\n"
            "USBD_FILE = %s\r\n"
            "NET_HOSTwrite = %d\r\n"
            "Menu_Title = %s\r\n"
            "Init_Delay = %d\r\n"
            "USBKBD_USED = %d\r\n"
            "USBKBD_FILE = %s\r\n"
            "KBDMAP_FILE = %s\r\n"
            "Menu_Show_Titles = %d\r\n"
            "PathPad_Lock = %d\r\n"
            "CNF_Path = %s\r\n"
            "USBMASS_FILE = %s\r\n"
            "LANG_FILE = %s\r\n"
            "FONT_FILE = %s\r\n"
            "JpgView_Timer = %d\r\n"
            "JpgView_Trans = %d\r\n"
            "JpgView_Full = %d\r\n"
            "PSU_HugeNames = %d\r\n"
            "PSU_DateNames = %d\r\n"
            "PSU_NoOverwrite = %d\r\n"
            "FB_NoIcons = %d\r\n"
            "%n",                      // %n causes NO output, but only a measurement
            setting->timeout,          //auto_Timer
            setting->Hide_Paths,       //Menu_Hide_Paths
            setting->numCNF,           //Menu_Pages
            setting->swapKeys,         //GUI_Swap_Keys
            setting->usbd_file,        //USBD_FILE
            setting->HOSTwrite,        //NET_HOST_write
            setting->Menu_Title,       //Menu_Title
            setting->Init_Delay,       //Init_Delay
            setting->usbkbd_used,      //USBKBD_USED
            setting->usbkbd_file,      //USBKBD_FILE
            setting->kbdmap_file,      //KBDMAP_FILE
            setting->Show_Titles,      //Menu_Show_Titles
            setting->PathPad_Lock,     //PathPad_Lock
            setting->CNF_Path,         //CNF_Path
            setting->usbmass_file,     //USBMASS_FILE
            setting->lang_file,        //LANG_FILE
            setting->font_file,        //FONT_FILE
            setting->JpgView_Timer,    //JpgView_Timer
            setting->JpgView_Trans,    //JpgView_Trans
            setting->JpgView_Full,     //JpgView_Full
            setting->PSU_HugeNames,    //PSU_HugeNames
            setting->PSU_DateNames,    //PSU_DateNames
            setting->PSU_NoOverwrite,  //PSU_NoOverwrite
            setting->FB_NoIcons,       //FB_NoIcons
            &CNF_step                  // This variable measures the size of sprintf data
            );
    CNF_size += CNF_step;

    for (i = 0; i < 16; i++) {          //Loop to save user defined launch key titles
        if (setting->LK_Title[i][0]) {  //Only save non-empty strings
            sprintf(tmp + CNF_size,
                    "LK_%s_Title = %s\r\n"
                    "%n",  // %n causes NO output, but only a measurement
                    LK_ID[i],
                    setting->LK_Title[i],
                    &CNF_step  // This variable measures the size of sprintf data
                    );
            CNF_size += CNF_step;
        }  //ends if
    }      //ends for

    sprintf(tmp + CNF_size,
            "PathPad_Lock = %d\r\n"
            "%n",                   // %n causes NO output, but only a measurement
            setting->PathPad_Lock,  //PathPad_Lock
            &CNF_step               // This variable measures the size of sprintf data
            );
    CNF_size += CNF_step;

    for (i = 0; i < 30; i++) {  //Loop to save non-empty PathPad entries
        if (PathPad[i][0]) {    //Only save non-empty strings
            sprintf(tmp + CNF_size,
                    "PathPad[%02d] = %s\r\n"
                    "%n",  // %n causes NO output, but only a measurement
                    i,
                    PathPad[i],
                    &CNF_step  // This variable measures the size of sprintf data
                    );
            CNF_size += CNF_step;
        }  //ends if
    }      //ends for

    strcpy(c, LaunchElfDir);
    strcat(c, CNF);
    ret = genFixPath(c, cnf_path);
    if ((ret >= 0) && ((fd = genOpen(cnf_path, O_RDONLY)) >= 0))
        genClose(fd);
    else {                                //Start of clause for failure to use LaunchElfDir
        if (setting->CNF_Path[0] == 0) {  //if NO CNF Path override defined
            if (!strncmp(LaunchElfDir, "mc", 2))
                sprintf(c, "mc%d:/SYS-CONF", LaunchElfDir[2] - '0');
            else
                sprintf(c, "mc%d:/SYS-CONF", CheckMC());

            if ((fd = fileXioDopen(c)) >= 0) {
                fileXioDclose(fd);
                char strtmp[MAX_PATH] = "/";
                strcat(c, strcat(strtmp, CNF));
            } else {
                strcpy(c, LaunchElfDir);
                strcat(c, CNF);
            }
        }
    }  //End of clause for failure to use LaunchElfDir

    ret = genFixPath(c, cnf_path);
    if ((ret < 0) || ((fd = genOpen(cnf_path, O_CREAT | O_WRONLY | O_TRUNC)) < 0)) {
        sprintf(c, "mc%d:/SYS-CONF", CheckMC());
        if ((fd = fileXioDopen(c)) >= 0) {
            fileXioDclose(fd);
            char strtmp[MAX_PATH] = "/";
            strcat(c, strcat(strtmp, CNF));
        } else {
            strcpy(c, LaunchElfDir);
            strcat(c, CNF);
        }
        ret = genFixPath(c, cnf_path);
        if ((fd = genOpen(cnf_path, O_CREAT | O_WRONLY | O_TRUNC)) < 0) {
            sprintf(mainMsg, "%s %s", LNG(Failed_To_Save), CNF);
            return;
        }
    }
    ret = genWrite(fd, &tmp, CNF_size);
    if (ret == CNF_size)
        sprintf(mainMsg, "%s (%s)", LNG(Saved_Config), c);
    else
        sprintf(mainMsg, "%s (%s)", LNG(Failed_writing), CNF);
    genClose(fd);
}
//---------------------------------------------------------------------------
void initConfig(void)
{
    int i;

    if (setting != NULL)
        free(setting);
    setting = (SETTING *)malloc(sizeof(SETTING));

    sprintf(setting->Misc, "%s/", LNG_DEF(MISC));
    sprintf(setting->Misc_PS2Disc, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Disc));
    sprintf(setting->Misc_FileBrowser, "%s/%s", LNG_DEF(MISC), LNG_DEF(FileBrowser));
    sprintf(setting->Misc_PS2Browser, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Browser));
    sprintf(setting->Misc_PS2Net, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2Net));
    sprintf(setting->Misc_PS2PowerOff, "%s/%s", LNG_DEF(MISC), LNG_DEF(PS2PowerOff));
    sprintf(setting->Misc_HddManager, "%s/%s", LNG_DEF(MISC), LNG_DEF(HddManager));
    sprintf(setting->Misc_TextEditor, "%s/%s", LNG_DEF(MISC), LNG_DEF(TextEditor));
    sprintf(setting->Misc_JpgViewer, "%s/%s", LNG_DEF(MISC), LNG_DEF(JpgViewer));
    sprintf(setting->Misc_Configure, "%s/%s", LNG_DEF(MISC), LNG_DEF(Configure));
    sprintf(setting->Misc_Load_CNFprev, "%s/%s", LNG_DEF(MISC), LNG_DEF(Load_CNFprev));
    sprintf(setting->Misc_Load_CNFnext, "%s/%s", LNG_DEF(MISC), LNG_DEF(Load_CNFnext));
    sprintf(setting->Misc_Set_CNF_Path, "%s/%s", LNG_DEF(MISC), LNG_DEF(Set_CNF_Path));
    sprintf(setting->Misc_Load_CNF, "%s/%s", LNG_DEF(MISC), LNG_DEF(Load_CNF));
    sprintf(setting->Misc_ShowFont, "%s/%s", LNG_DEF(MISC), LNG_DEF(ShowFont));
    sprintf(setting->Misc_Debug_Info, "%s/%s", LNG_DEF(MISC), LNG_DEF(Debug_Info));
    sprintf(setting->Misc_About_uLE, "%s/%s", LNG_DEF(MISC), LNG_DEF(About_uLE));
    sprintf(setting->Misc_OSDSYS, "%s/%s", LNG_DEF(MISC), LNG_DEF(OSDSYS));

    for (i = 0; i < 17; i++) {
        setting->LK_Path[i][0] = 0;
        setting->LK_Title[i][0] = 0;
        setting->LK_Flag[i] = 0;
    }
    for (i = 0; i < 30; i++)
        PathPad[i][0] = 0;

    strcpy(setting->LK_Path[1], setting->Misc_FileBrowser);
    setting->LK_Flag[1] = 1;
    strcpy(setting->LK_Path[4], setting->Misc_About_uLE);
    setting->LK_Flag[4] = 1;
    setting->usbd_file[0] = '\0';
    setting->usbmass_file[0] = '\0';
    setting->usbkbd_file[0] = '\0';
    setting->kbdmap_file[0] = '\0';
    setting->skin[0] = '\0';
    setting->GUI_skin[0] = '\0';
    setting->Menu_Title[0] = '\0';
    setting->CNF_Path[0] = '\0';
    setting->lang_file[0] = '\0';
    setting->font_file[0] = '\0';
    setting->timeout = DEF_TIMEOUT;
    setting->Hide_Paths = DEF_HIDE_PATHS;
    setting->color[COLOR_BACKGR] = DEF_COLOR1;
    setting->color[COLOR_FRAME] = DEF_COLOR2;
    setting->color[COLOR_SELECT] = DEF_COLOR3;
    setting->color[COLOR_TEXT] = DEF_COLOR4;
    setting->color[COLOR_GRAPH1] = DEF_COLOR5;
    setting->color[COLOR_GRAPH2] = DEF_COLOR6;
    setting->color[COLOR_GRAPH3] = DEF_COLOR7;
    setting->color[COLOR_GRAPH4] = DEF_COLOR8;
    setting->screen_x = 0;
    setting->screen_y = 0;
    setting->Menu_Frame = DEF_MENU_FRAME;
    setting->Show_Menu = DEF_MENU;
    setting->numCNF = DEF_NUMCNF;
    setting->swapKeys = DEF_SWAPKEYS;
    setting->HOSTwrite = DEF_HOSTWRITE;
    setting->Brightness = DEF_BRIGHT;
    setting->TV_mode = TV_mode_AUTO;
    setting->Popup_Opaque = DEF_POPUP_OPAQUE;
    setting->Init_Delay = DEF_INIT_DELAY;
    setting->usbkbd_used = DEF_USBKBD_USED;
    setting->Show_Titles = DEF_SHOW_TITLES;
    setting->PathPad_Lock = DEF_PATHPAD_LOCK;
    setting->JpgView_Timer = -1;  //only used to detect missing variable
    setting->JpgView_Trans = -1;  //only used to detect missing variable
    setting->JpgView_Full = DEF_JPGVIEW_FULL;
    setting->PSU_HugeNames = DEF_PSU_HUGENAMES;
    setting->PSU_DateNames = DEF_PSU_DATENAMES;
    setting->PSU_NoOverwrite = DEF_PSU_NOOVERWRITE;
    setting->FB_NoIcons = DEF_FB_NOICONS;
}
//------------------------------
//endfunc initConfig
//---------------------------------------------------------------------------
// Load LAUNCHELF.CNF (or LAUNCHELFx.CNF with multiple pages)
// sincro: ADD load USBD_FILE string
// polo: ADD load SKIN_FILE string
// suloku: ADD load MAIN_SKIN string //dlanor: changed to GUI_SKIN_FILE
// dlanor: added error flag return value 0==OK, -1==failure
//---------------------------------------------------------------------------
int loadConfig(char *mainMsg, char *CNF)
{
    int i, fd, tst, len, mcport, var_cnt, CNF_version;
    char tsts[20];
    char path[MAX_PATH];
    char cnf_path[MAX_PATH];
    char *RAM_p, *CNF_p, *name, *value;

    initConfig();

    strcpy(path, LaunchElfDir);
    strcat(path, CNF);
    if (!strncmp(path, "cdrom", 5))
        strcat(path, ";1");

    fd = -1;
    if ((tst = genFixPath(path, cnf_path)) >= 0)
        fd = genOpen(cnf_path, O_RDONLY);
    if (fd < 0) {
        char strtmp[MAX_PATH], *p;
        int pos;

        p = strrchr(path, '.');  //make p point to extension
        if (*(p - 1) != 'F')     //is this an indexed CNF
            p--;                 //then make p point to index
        pos = (p - path);
        strcpy(strtmp, path);
        strcpy(strtmp + pos - 9, "LNCHELF");   //Replace LAUNCHELF with LNCHELF (for CD)
        strcpy(strtmp + pos - 2, path + pos);  //Add index+extension too
        if ((tst = genFixPath(strtmp, cnf_path)) >= 0)
            fd = genOpen(cnf_path, O_RDONLY);
        if (fd < 0) {
            if (!strncmp(LaunchElfDir, "mc", 2))
                mcport = LaunchElfDir[2] - '0';
            else
                mcport = CheckMC();
            if (mcport == 1 || mcport == 0) {
                sprintf(strtmp, "mc%d:/SYS-CONF/", mcport);
                strcpy(cnf_path, strtmp);
                strcat(cnf_path, CNF);
                fd = genOpen(cnf_path, O_RDONLY);
                if (fd >= 0)
                    strcpy(LaunchElfDir, strtmp);
            }
        }
    }
    if (fd < 0) {
    failed_load:
        sprintf(mainMsg, "%s %s", LNG(Failed_To_Load), CNF);
        return -1;
    }
    // This point is only reached after succefully opening CNF
    genClose(fd);

    if ((RAM_p = preloadCNF(cnf_path)) == NULL)
        goto failed_load;
    CNF_p = RAM_p;

    //RA NB: in the code below, the 'LK_' variables have been implemented such that
    //       any _Ex suffix will be accepted, with identical results. This will need
    //       to be modified when more execution methods are implemented.

    CNF_version = 0;                                                       // The CNF version is still unidentified
    for (var_cnt = 0; get_CNF_string(&CNF_p, &name, &value); var_cnt++) {  // A variable was found, now we dispose of its value.
        if (!strcmp(name, "CNF_version")) {
            CNF_version = atoi(value);
            continue;
        } else if (CNF_version == 0)
            goto failed_load;  // Refuse unidentified CNF

        if (scanSkinCNF(name, value))
            continue;

        for (i = 0; i < 17; i++) {
            sprintf(tsts, "LK_%s_E%n", LK_ID[i], &len);
            if (!strncmp(name, tsts, len)) {
                strcpy(setting->LK_Path[i], value);
                setting->LK_Flag[i] = 1;
                break;
            }
        }
        if (i < 17)
            continue;
        //----------
        //In the next group, the Misc device must be defined before its subprograms
        //----------
        else if (!strcmp(name, "Misc"))
            sprintf(setting->Misc, "%s/", value);
        else if (!strcmp(name, "Misc_PS2Disc"))
            sprintf(setting->Misc_PS2Disc, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_FileBrowser"))
            sprintf(setting->Misc_FileBrowser, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_PS2Browser"))
            sprintf(setting->Misc_PS2Browser, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_PS2Net"))
            sprintf(setting->Misc_PS2Net, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_PS2PowerOff"))
            sprintf(setting->Misc_PS2PowerOff, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_HddManager"))
            sprintf(setting->Misc_HddManager, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_TextEditor"))
            sprintf(setting->Misc_TextEditor, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_JpgViewer"))
            sprintf(setting->Misc_JpgViewer, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_Configure"))
            sprintf(setting->Misc_Configure, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_Load_CNFprev"))
            sprintf(setting->Misc_Load_CNFprev, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_Load_CNFnext"))
            sprintf(setting->Misc_Load_CNFnext, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_Set_CNF_Path"))
            sprintf(setting->Misc_Set_CNF_Path, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_Load_CNF"))
            sprintf(setting->Misc_Load_CNF, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_ShowFont"))
            sprintf(setting->Misc_ShowFont, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_Debug_Info"))
            sprintf(setting->Misc_Debug_Info, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_About_uLE"))
            sprintf(setting->Misc_About_uLE, "%s%s", setting->Misc, value);
        else if (!strcmp(name, "Misc_OSDSYS"))
            sprintf(setting->Misc_OSDSYS, "%s%s", setting->Misc, value);
        //----------
        else if (!strcmp(name, "LK_auto_Timer"))
            setting->timeout = atoi(value);
        else if (!strcmp(name, "Menu_Hide_Paths"))
            setting->Hide_Paths = atoi(value);
        //---------- NB: color settings moved to scanSkinCNF
        else if (!strcmp(name, "Menu_Pages"))
            setting->numCNF = atoi(value);
        else if (!strcmp(name, "GUI_Swap_Keys"))
            setting->swapKeys = atoi(value);
        else if (!strcmp(name, "USBD_FILE"))
            strcpy(setting->usbd_file, value);
        else if (!strcmp(name, "NET_HOSTwrite"))
            setting->HOSTwrite = atoi(value);
        else if (!strcmp(name, "Menu_Title")) {
            strncpy(setting->Menu_Title, value, MAX_MENU_TITLE);
            setting->Menu_Title[MAX_MENU_TITLE] = '\0';
        } else if (!strcmp(name, "Init_Delay"))
            setting->Init_Delay = atoi(value);
        else if (!strcmp(name, "USBKBD_USED"))
            setting->usbkbd_used = atoi(value);
        else if (!strcmp(name, "USBKBD_FILE"))
            strcpy(setting->usbkbd_file, value);
        else if (!strcmp(name, "KBDMAP_FILE"))
            strcpy(setting->kbdmap_file, value);
        else if (!strcmp(name, "Menu_Show_Titles"))
            setting->Show_Titles = atoi(value);
        else if (!strcmp(name, "PathPad_Lock"))
            setting->PathPad_Lock = atoi(value);
        else if (!strcmp(name, "CNF_Path"))
            strcpy(setting->CNF_Path, value);
        else if (!strcmp(name, "USBMASS_FILE"))
            strcpy(setting->usbmass_file, value);
        else if (!strcmp(name, "LANG_FILE"))
            strcpy(setting->lang_file, value);
        else if (!strcmp(name, "FONT_FILE"))
            strcpy(setting->font_file, value);
        //----------
        else if (!strcmp(name, "JpgView_Timer"))
            setting->JpgView_Timer = atoi(value);
        else if (!strcmp(name, "JpgView_Trans"))
            setting->JpgView_Trans = atoi(value);
        else if (!strcmp(name, "JpgView_Full"))
            setting->JpgView_Full = atoi(value);
        //----------
        else if (!strcmp(name, "PSU_HugeNames"))
            setting->PSU_HugeNames = atoi(value);
        else if (!strcmp(name, "PSU_DateNames"))
            setting->PSU_DateNames = atoi(value);
        else if (!strcmp(name, "PSU_NoOverwrite"))
            setting->PSU_NoOverwrite = atoi(value);
        else if (!strcmp(name, "FB_NoIcons"))
            setting->FB_NoIcons = atoi(value);
        //----------
        else {
            for (i = 0; i < 16; i++) {
                sprintf(tsts, "LK_%s_Title", LK_ID[i]);
                if (!strcmp(name, tsts)) {
                    strncpy(setting->LK_Title[i], value, MAX_ELF_TITLE - 1);
                    break;
                }
            }
            if (i < 16)
                continue;
            else if (!strncmp(name, "PathPad[", 8)) {
                i = atoi(name + 8);
                if (i < 30) {
                    strncpy(PathPad[i], value, MAX_PATH - 1);
                    PathPad[i][MAX_PATH - 1] = '\0';
                }
            }
        }
    }  //ends for
    for (i = 0; i < 16; i++)
        setting->LK_Title[i][MAX_ELF_TITLE - 1] = 0;
    free(RAM_p);
    if (setting->JpgView_Timer < 0)
        setting->JpgView_Timer = DEF_JPGVIEW_TIMER;
    if ((setting->JpgView_Trans < 1) || (setting->JpgView_Trans > 4))
        setting->JpgView_Trans = DEF_JPGVIEW_TRANS;
    sprintf(mainMsg, "%s (%s)", LNG(Loaded_Config), path);
    return 0;
}
//------------------------------
//endfunc loadConfig
//---------------------------------------------------------------------------
// Polo: ADD Skin Menu with Skin preview
// suloku: ADD Main skin selection
//---------------------------------------------------------------------------
void Config_Skin(void)
{
    int s, max_s = 7;
    int x, y;
    int event, post_event = 0;
    char c[MAX_PATH];
    char skinSave[MAX_PATH], GUI_Save[MAX_PATH];
    int Brightness = setting->Brightness;
    int current_preview = 0;

    strcpy(skinSave, setting->skin);
    strcpy(GUI_Save, setting->GUI_skin);

    loadSkin(PREVIEW_PIC, 0, 0);
    current_preview = PREVIEW_PIC;

    s = 1;
    event = 1;  //event = initial entry
    while (1) {
        //Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_UP) {
                event |= 2;  //event |= valid pad command
                if (s != 1)
                    s--;
                else
                    s = max_s;
            } else if (new_pad & PAD_DOWN) {
                event |= 2;  //event |= valid pad command
                if (s != max_s)
                    s++;
                else
                    s = 1;
            } else if (new_pad & PAD_LEFT) {
                event |= 2;  //event |= valid pad command
                if (s != 1)
                    s = 1;
                else
                    s = max_s;
            } else if (new_pad & PAD_RIGHT) {
                event |= 2;  //event |= valid pad command
                if (s != max_s)
                    s = max_s;
                else
                    s = 1;
            } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;    //event |= valid pad command
                if (s == 1) {  //Command == Cancel Skin Path
                    setting->skin[0] = '\0';
                    loadSkin(PREVIEW_PIC, 0, 0);
                    current_preview = PREVIEW_PIC;
                } else if (s == 3) {  //Command == Decrease Brightness
                    if ((Brightness > 0) && (testsetskin == 1)) {
                        Brightness--;
                    }
                } else if (s == 4) {  //Command == Cancel GUI Skin Path
                    setting->GUI_skin[0] = '\0';
                    loadSkin(PREVIEW_GUI, 0, 0);
                    current_preview = PREVIEW_GUI;
                }
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;    //event |= valid pad command
                if (s == 1) {  //Command == Set Skin Path
                    getFilePath(setting->skin, SKIN_CNF);
                    loadSkin(PREVIEW_PIC, 0, 0);
                    current_preview = PREVIEW_PIC;
                } else if (s == 2) {  //Command == Apply New Skin
                    GUI_active = 0;
                    loadSkin(BACKGROUND_PIC, 0, 0);
                    setting->Brightness = Brightness;
                    strcpy(skinSave, setting->skin);
                    loadSkin(PREVIEW_PIC, 0, 0);
                    current_preview = PREVIEW_PIC;
                } else if (s == 3) {  //Command == Increase Brightness
                    if ((Brightness < 100) && (testsetskin == 1)) {
                        Brightness++;
                    }
                } else if (s == 4) {  //Command == Set GUI Skin Path
                    getFilePath(setting->GUI_skin, GUI_SKIN_CNF);
                    loadSkin(PREVIEW_GUI, 0, 0);
                    current_preview = PREVIEW_GUI;
                } else if (s == 5) {  //Command == Apply GUI Skin
                    strcpy(GUI_Save, setting->GUI_skin);
                    loadSkin(PREVIEW_GUI, 0, 0);
                    current_preview = PREVIEW_GUI;
                } else if (s == 6) {  //Command == Show GUI Menu
                    setting->Show_Menu = !setting->Show_Menu;
                } else if (s == 7) {  //Command == RETURN
                    setting->skin[0] = '\0';
                    strcpy(setting->skin, skinSave);
                    setting->GUI_skin[0] = '\0';
                    strcpy(setting->GUI_skin, GUI_Save);
                    return;
                }
            } else if (new_pad & PAD_TRIANGLE) {
                setting->skin[0] = '\0';
                strcpy(setting->skin, skinSave);
                setting->GUI_skin[0] = '\0';
                strcpy(setting->GUI_skin, GUI_Save);
                return;
            }
        }  //end if(readpad())

        if (event || post_event) {  //NB: We need to update two frame buffers per event

            //Display section
            clrScr(setting->color[COLOR_BACKGR]);

            if (testsetskin == 1) {
                setBrightness(Brightness);
                gsKit_prim_sprite_texture(gsGlobal, &TexPreview,
                                          SCREEN_WIDTH / 4, (SCREEN_HEIGHT / 4) + 60, 0, 0,
                                          (SCREEN_WIDTH / 4) * 3, ((SCREEN_HEIGHT / 4) * 3) + 60, SCREEN_WIDTH, SCREEN_HEIGHT,
                                          0, BrightColor);
                setBrightness(50);
            } else {
                gsKit_prim_sprite(gsGlobal,
                                  SCREEN_WIDTH / 4, (SCREEN_HEIGHT / 4) + 60, (SCREEN_WIDTH / 4) * 3, ((SCREEN_HEIGHT / 4) * 3) + 60,
                                  0, setting->color[COLOR_BACKGR]);
            }
            drawFrame((SCREEN_WIDTH / 4) - 2, ((SCREEN_HEIGHT / 4) + 60) - 1,
                      ((SCREEN_WIDTH / 4) * 3) + 1, ((SCREEN_HEIGHT / 4) * 3) + 60,
                      setting->color[COLOR_FRAME]);

            x = Menu_start_x;
            y = Menu_start_y;

            printXY(LNG(SKIN_SETTINGS), x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->skin) == 0)
                sprintf(c, "  %s: %s", LNG(Skin_Path), LNG(NULL));
            else
                sprintf(c, "  %s: %s", LNG(Skin_Path), setting->skin);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s", LNG(Apply_New_Skin));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s: %d", LNG(Brightness), Brightness);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->GUI_skin) == 0)
                sprintf(c, "  %s %s: %s", LNG(GUI), LNG(Skin_Path), LNG(NULL));
            else
                sprintf(c, "  %s %s: %s", LNG(GUI), LNG(Skin_Path), setting->GUI_skin);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s", LNG(Apply_GUI_Skin));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (setting->Show_Menu)
                sprintf(c, "  %s: %s", LNG(Show_Menu), LNG(ON));
            else
                sprintf(c, "  %s: %s", LNG(Show_Menu), LNG(OFF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s", LNG(RETURN));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (current_preview == PREVIEW_PIC)
                sprintf(c, "%s ", LNG(Normal));
            else
                sprintf(c, "%s ", LNG(GUI));
            strcat(c, LNG(Skin_Preview));
            printXY(c, SCREEN_WIDTH / 4, (SCREEN_HEIGHT / 4) + 78 - FONT_HEIGHT, setting->color[COLOR_TEXT], TRUE, 0);

            //Cursor positioning section
            y = Menu_start_y + s * (FONT_HEIGHT);
            drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

            //Tooltip section
            if ((s == 1) || (s == 4)) {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s", LNG(Edit), LNG(Clear));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s", LNG(Edit), LNG(Clear));
            } else if (s == 3) {  //if cursor at a colour component or a screen offset
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s", LNG(Add), LNG(Subtract));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s", LNG(Add), LNG(Subtract));
            } else if (s == 6) {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(Change));
                else
                    sprintf(c, "\xFF""0:%s", LNG(Change));
            } else {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(OK));
                else
                    sprintf(c, "\xFF""0:%s", LNG(OK));
            }
            sprintf(tmp, " \xFF""3:%s", LNG(Return));
            strcat(c, tmp);
            setScrTmp("", c);
        }  //ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;

    }  //ends while
}  //ends Config_Skin
//---------------------------------------------------------------------------
void Config_Screen(void)
{
    int i;
    int s, max_s = 35;  //define cursor index and its max value
    int x, y;
    int event, post_event = 0;
    u8 rgb[COLOR_COUNT][3];
    char c[MAX_PATH];
    int space = ((SCREEN_WIDTH - SCREEN_MARGIN - 4 * FONT_WIDTH) - (Menu_start_x + 2 * FONT_WIDTH)) / 8;

    event = 1;  //event = initial entry

    for (i = 0; i < COLOR_COUNT; i++) {
        rgb[i][0] = setting->color[i] & 0xFF;
        rgb[i][1] = setting->color[i] >> 8 & 0xFF;
        rgb[i][2] = setting->color[i] >> 16 & 0xFF;
    }

    s = 0;
    while (1) {
        //Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_UP) {
                event |= 2;  //event |= valid pad command
                if (s == 0)
                    s = max_s;
                else if (s == 24)
                    s = 2;
                else
                    s--;
            } else if (new_pad & PAD_DOWN) {
                event |= 2;  //event |= valid pad command
                if ((s < 24) && (s % 3 == 2))
                    s = 24;
                else if (s == max_s)
                    s = 0;
                else
                    s++;
            } else if (new_pad & PAD_LEFT) {
                event |= 2;  //event |= valid pad command
                if (s >= 34)
                    s = 32;
                else if (s >= 32)
                    s = 31;
                else if (s >= 31)
                    s = 28;
                else if (s >= 28)
                    s = 27;
                else if (s >= 27)
                    s = 25;
                else if (s >= 25)
                    s = 24;  //at or
                else if (s >= 24)
                    s = 21;  //if s beyond color settings
                else if (s >= 3)
                    s -= 3;  //if s in a color beyond Color1 step to preceding color
            } else if (new_pad & PAD_RIGHT) {
                event |= 2;  //event |= valid pad command
                if (s >= 32)
                    s = 34;
                else if (s >= 31)
                    s = 32;
                else if (s >= 28)
                    s = 31;
                else if (s >= 27)
                    s = 28;
                else if (s >= 25)
                    s = 27;
                else if (s >= 24)
                    s = 25;
                else if (s >= 21)
                    s = 24;  //if s in Color8, move it to ScreenX
                else
                    s += 3;                                                                         //if s in a color before Color8, step to next color
            } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {  //User pressed CANCEL=>Subtract/Clear
                event |= 2;                                                                         //event |= valid pad command
                if (s < 24) {
                    if (rgb[s / 3][s % 3] > 0) {
                        rgb[s / 3][s % 3]--;
                        setting->color[s / 3] =
                            GS_SETREG_RGBA(rgb[s / 3][0], rgb[s / 3][1], rgb[s / 3][2], 0);
                    }
                } else if (s == 25) {
                    if (setting->screen_x > -gsGlobal->StartX) {
                        setting->screen_x--;
                        updateScreenMode();
                    }
                } else if (s == 26) {
                    if (setting->screen_y > -gsGlobal->StartY) {
                        setting->screen_y--;
                        updateScreenMode();
                    }
                } else if (s == 31) {  //cursor is at Menu_Title
                    setting->Menu_Title[0] = '\0';
                }
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  //User pressed OK=>Add/Ok/Edit
                event |= 2;                                                                         //event |= valid pad command
                if (s < 24) {
                    if (rgb[s / 3][s % 3] < 255) {
                        rgb[s / 3][s % 3]++;
                        setting->color[s / 3] =
                            GS_SETREG_RGBA(rgb[s / 3][0], rgb[s / 3][1], rgb[s / 3][2], 0);
                    }
                } else if (s == 24) {
                    setting->TV_mode = (setting->TV_mode + 1) % TV_mode_COUNT;  //Change between the various modes
                    updateScreenMode();
                } else if (s == 25) {
                    if (setting->screen_x < gsGlobal->StartX) {
                      setting->screen_x++;
                      updateScreenMode();
                    }
                } else if (s == 26) {
                    if (setting->screen_y < gsGlobal->StartY) {
                      setting->screen_y++;
                      updateScreenMode();
                    }
                } else if (s == 27) {
                    setting->interlace = !setting->interlace;
                    updateScreenMode();
                } else if (s == 28) {
                    Config_Skin();
                } else if (s == 29) {
                    loadSkinBrowser();
                } else if (s == 30) {
                    saveSkinBrowser();
                } else if (s == 31) {  //cursor is at Menu_Title
                    char tmp[MAX_MENU_TITLE + 1];
                    strcpy(tmp, setting->Menu_Title);
                    if (keyboard(tmp, 36) >= 0)
                        strcpy(setting->Menu_Title, tmp);
                } else if (s == 32) {
                    setting->Menu_Frame = !setting->Menu_Frame;
                } else if (s == 33) {
                    setting->Popup_Opaque = !setting->Popup_Opaque;
                } else if (s == max_s - 1) {  //Always put 'RETURN' next to last
                    return;
                } else if (s == max_s) {  //Always put 'DEFAULT SCREEN SETTINGS' last
                    setting->skin[0] = '\0';
                    setting->GUI_skin[0] = '\0';
                    loadSkin(BACKGROUND_PIC, 0, 0);
                    setting->color[COLOR_BACKGR] = DEF_COLOR1;
                    setting->color[COLOR_FRAME] = DEF_COLOR2;
                    setting->color[COLOR_SELECT] = DEF_COLOR3;
                    setting->color[COLOR_TEXT] = DEF_COLOR4;
                    setting->color[COLOR_GRAPH1] = DEF_COLOR5;
                    setting->color[COLOR_GRAPH2] = DEF_COLOR6;
                    setting->color[COLOR_GRAPH3] = DEF_COLOR7;
                    setting->color[COLOR_GRAPH4] = DEF_COLOR8;
                    setting->TV_mode = TV_mode_AUTO;
                    setting->screen_x = 0;
                    setting->screen_y = 0;
                    setting->Menu_Frame = DEF_MENU_FRAME;
                    setting->Show_Menu = DEF_MENU;
                    setting->Brightness = DEF_BRIGHT;
                    setting->Popup_Opaque = DEF_POPUP_OPAQUE;
                    updateScreenMode();

                    for (i = 0; i < COLOR_COUNT; i++) {
                        rgb[i][0] = setting->color[i] & 0xFF;
                        rgb[i][1] = setting->color[i] >> 8 & 0xFF;
                        rgb[i][2] = setting->color[i] >> 16 & 0xFF;
                    }
                }
            } else if (new_pad & PAD_TRIANGLE)
                return;
        }

        if (event || post_event) {  //NB: We need to update two frame buffers per event

            //Display section
            clrScr(setting->color[COLOR_BACKGR]);

            x = Menu_start_x;

            for (i = 0; i < 8; i++) {
                y = Menu_start_y;
                sprintf(c, "%s%d", LNG(Color), i + 1);
                printXY(c, x + (space * (i + 1)) - (printXY(c, 0, 0, 0, FALSE, space - FONT_WIDTH / 2) / 2), y,
                        setting->color[COLOR_TEXT], TRUE, space - FONT_WIDTH / 2);
                if (i == 0)
                    sprintf(c, "%s", LNG(Backgr));
                else if (i == 1)
                    sprintf(c, "%s", LNG(Frames));
                else if (i == 2)
                    sprintf(c, "%s", LNG(Select));
                else if (i == 3)
                    sprintf(c, "%s", LNG(Normal));
                else if (i >= 4)
                    sprintf(c, "%s%d", LNG(Graph), i - 3);
                printXY(c, x + (space * (i + 1)) - (printXY(c, 0, 0, 0, FALSE, space - FONT_WIDTH / 2) / 2), y + FONT_HEIGHT,
                        setting->color[COLOR_TEXT], TRUE, space - FONT_WIDTH / 2);
                y += FONT_HEIGHT * 2;
                printXY("R:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
                sprintf(c, "%02X", rgb[i][0]);
                printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
                y += FONT_HEIGHT;
                printXY("G:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
                sprintf(c, "%02X", rgb[i][1]);
                printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
                y += FONT_HEIGHT;
                printXY("B:", x, y, setting->color[COLOR_TEXT], TRUE, 0);
                sprintf(c, "%02X", rgb[i][2]);
                printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
                y += FONT_HEIGHT;
                sprintf(c, "\xFF""4");
                printXY(c, x + (space * (i + 1)) - FONT_WIDTH, y, setting->color[i], TRUE, 0);
            }  //ends loop for colour RGB values
            y += FONT_HEIGHT * 2;
            sprintf(c, "  %s: ", LNG(TV_mode));
            if (setting->TV_mode == TV_mode_NTSC)
                strcat(c, "NTSC");
            else if (setting->TV_mode == TV_mode_PAL)
                strcat(c, "PAL");
            else if (setting->TV_mode == TV_mode_VGA)
                strcat(c, "VGA");
            else if (setting->TV_mode == TV_mode_480P)
                strcat(c, "Progressive");
            else
                strcat(c, "AUTO");
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            y += FONT_HEIGHT / 2;

            sprintf(c, "  %s: %d", LNG(Screen_X_offset), setting->screen_x);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(c, "  %s: %d", LNG(Screen_Y_offset), setting->screen_y);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            y += FONT_HEIGHT / 2;

            if (setting->interlace)
                sprintf(c, "  %s: %s", LNG(Interlace), LNG(ON));
            else
                sprintf(c, "  %s: %s", LNG(Interlace), LNG(OFF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            y += FONT_HEIGHT / 2;

            sprintf(c, "  %s...", LNG(Skin_Settings));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(c, "  %s...", LNG(Load_Skin_CNF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(c, "  %s...", LNG(Save_Skin_CNF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            y += FONT_HEIGHT / 2;

            if (setting->Menu_Title[0] == '\0')
                sprintf(c, "  %s: %s", LNG(Menu_Title), LNG(NULL));
            else
                sprintf(c, "  %s: %s", LNG(Menu_Title), setting->Menu_Title);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            y += FONT_HEIGHT / 2;

            if (setting->Menu_Frame)
                sprintf(c, "  %s: %s", LNG(Menu_Frame), LNG(ON));
            else
                sprintf(c, "  %s: %s", LNG(Menu_Frame), LNG(OFF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (setting->Popup_Opaque)
                sprintf(c, "  %s: %s", LNG(Popups_Opaque), LNG(ON));
            else
                sprintf(c, "  %s: %s", LNG(Popups_Opaque), LNG(OFF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            y += FONT_HEIGHT / 2;

            sprintf(c, "  %s", LNG(RETURN));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(c, "  %s", LNG(Use_Default_Screen_Settings));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            //Cursor positioning section
            x = Menu_start_x;
            y = Menu_start_y;

            if (s < 24) {  //if cursor indicates a colour component
                int colnum = s / 3;
                int comnum = s - colnum * 3;
                x += (space * (colnum + 1)) - (FONT_WIDTH * 4);
                y += (2 + comnum) * FONT_HEIGHT;
            } else {                                                //if cursor indicates anything after colour components
                y += (s - 24 + 6) * FONT_HEIGHT + FONT_HEIGHT / 2;  //adjust y for cursor beyond colours
                //Here y is almost correct, except for additional group spacing
                if (s >= 24)               //if cursor at or beyond TV mode choice
                    y += FONT_HEIGHT / 2;  //adjust for half-row space below colours
                if (s >= 25)               //if cursor at or beyond screen offsets
                    y += FONT_HEIGHT / 2;  //adjust for half-row space below TV mode choice
                if (s >= 27)               //if cursor at or beyond interlace choice
                    y += FONT_HEIGHT / 2;  //adjust for half-row space below screen offsets
                if (s >= 28)               //if cursor at or beyond 'SKIN SETTINGS'
                    y += FONT_HEIGHT / 2;  //adjust for half-row space below interlace choice
                if (s >= 31)               //if cursor at or beyond 'Menu Title'
                    y += FONT_HEIGHT / 2;  //adjust for half-row space below 'SKIN SETTINGS'
                if (s >= 32)               //if cursor at or beyond 'Menu Frame'
                    y += FONT_HEIGHT / 2;  //adjust for half-row space below 'Menu Title'
                if (s >= max_s - 1)        //if cursor at or beyond 'RETURN'
                    y += FONT_HEIGHT / 2;  //adjust for half-row space below 'Popups Opaque'
            }
            drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);  //draw cursor

            //Tooltip section
            if (s < 24 || s == 25 || s == 26) {  //if cursor at a colour component or a screen offset
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s", LNG(Add), LNG(Subtract));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s", LNG(Add), LNG(Subtract));
            } else if (s == 24 || s == 27 || s == 32 || s == 33) {
                //if cursor at 'TV mode', 'INTERLACE', 'Menu Frame' or 'Popups Opaque'
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(Change));
                else
                    sprintf(c, "\xFF""0:%s", LNG(Change));
            } else if (s == 28 || s == 29 || s == 30) {  //if cursor at 'SKIN SETTINGS'
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(OK));
                else
                    sprintf(c, "\xFF""0:%s", LNG(OK));
            } else if (s == 31) {  //if cursor at Menu_Title
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s", LNG(Edit), LNG(Clear));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s", LNG(Edit), LNG(Clear));
            } else {  //if cursor at 'RETURN' or 'DEFAULT' options
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(OK));
                else
                    sprintf(c, "\xFF""0:%s", LNG(OK));
            }
            sprintf(tmp, " \xFF""3:%s", LNG(Return));
            strcat(c, tmp);
            setScrTmp("", c);
        }  //ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;

    }  //ends while
}  //ends Config_Screen
//---------------------------------------------------------------------------
// Other settings by EP
// sincro: ADD USBD SELECTOR MENU
// dlanor: Add Menu_Title config
//---------------------------------------------------------------------------
void Config_Startup(void)
{
    int s, max_s = 15;  //define cursor index and its max value
    int x, y;
    int event, post_event = 0;
    char c[MAX_PATH];

    event = 1;  //event = initial entry
    s = 1;
    while (1) {
        //Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_UP) {
                event |= 2;  //event |= valid pad command
                if (s != 1)
                    s--;
                else
                    s = max_s;
            } else if (new_pad & PAD_DOWN) {
                event |= 2;  //event |= valid pad command
                if (s != max_s)
                    s++;
                else
                    s = 1;
            } else if (new_pad & PAD_LEFT) {
                event |= 2;  //event |= valid pad command
                if (s != max_s)
                    s = max_s;
                else
                    s = 1;
            } else if (new_pad & PAD_RIGHT) {
                event |= 2;  //event |= valid pad command
                if (s != max_s)
                    s = max_s;
                else
                    s = 1;
            } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;  //event |= valid pad command
                if (s == 1 && setting->numCNF > 1)
                    setting->numCNF--;
                else if (s == 3)
                    setting->usbd_file[0] = '\0';
                else if (s == 4 && setting->Init_Delay > 0)
                    setting->Init_Delay--;
                else if (s == 5 && setting->timeout > 0)
                    setting->timeout--;
                else if (s == 7)
                    setting->usbkbd_file[0] = '\0';
                else if (s == 8)
                    setting->kbdmap_file[0] = '\0';
                else if (s == 9)
                    setting->CNF_Path[0] = '\0';
                else if (s == 10)
                    setting->usbmass_file[0] = '\0';
                else if (s == 11) {
                    setting->lang_file[0] = '\0';
                    Load_External_Language();
                } else if (s == 12) {
                    setting->font_file[0] = '\0';
                    loadFont("");
                } else if (s == 13) {  //clear ESR file choice
                    setting->LK_Path[15][0] = 0;
                    setting->LK_Flag[15] = 0;
                } else if (s == 14) {  //clear OSDSYS file choice
                    setting->LK_Path[16][0] = 0;
                    setting->LK_Flag[16] = 0;
                }
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;  //event |= valid pad command
                if (s == 1)
                    setting->numCNF++;
                else if (s == 2)
                    setting->swapKeys = !setting->swapKeys;
                else if (s == 3)
                    getFilePath(setting->usbd_file, USBD_IRX_CNF);
                else if (s == 4)
                    setting->Init_Delay++;
                else if (s == 5)
                    setting->timeout++;
                else if (s == 6)
                    setting->usbkbd_used = !setting->usbkbd_used;
                else if (s == 7)
                    getFilePath(setting->usbkbd_file, USBKBD_IRX_CNF);
                else if (s == 8)
                    getFilePath(setting->kbdmap_file, KBDMAP_FILE_CNF);
                else if (s == 9) {
                    char *tmp;

                    getFilePath(setting->CNF_Path, CNF_PATH_CNF);
                    if ((tmp = strrchr(setting->CNF_Path, '/')))
                        tmp[1] = '\0';
                } else if (s == 10)
                    getFilePath(setting->usbmass_file, USBMASS_IRX_CNF);
                else if (s == 11) {
                    getFilePath(setting->lang_file, LANG_CNF);
                    Load_External_Language();
                } else if (s == 12) {
                    getFilePath(setting->font_file, FONT_CNF);
                    if (loadFont(setting->font_file) == 0)
                        setting->font_file[0] = '\0';
                } else if (s == 13) {  //Make ESR file choice
                    getFilePath(setting->LK_Path[15], LK_ELF_CNF);
                    if (!strncmp(setting->LK_Path[15], "mc0", 3) ||
                        !strncmp(setting->LK_Path[15], "mc1", 3)) {
                        sprintf(c, "mc%s", &setting->LK_Path[15][3]);
                        strcpy(setting->LK_Path[15], c);
                    }
                    if (setting->LK_Path[15][0])
                        setting->LK_Flag[15] = 1;
                } else if (s == 14) {  //Make OSDSYS file choice
                    getFilePath(setting->LK_Path[16], TEXT_CNF);
                    if (!strncmp(setting->LK_Path[16], "mc0", 3) ||
                        !strncmp(setting->LK_Path[16], "mc1", 3)) {
                        sprintf(c, "mc%s", &setting->LK_Path[16][3]);
                        strcpy(setting->LK_Path[16], c);
                    }
                    if (setting->LK_Path[16][0])
                        setting->LK_Flag[16] = 1;
                } else if (s == max_s)
                    return;
            } else if (new_pad & PAD_TRIANGLE)
                return;
        }

        if (event || post_event) {  //NB: We need to update two frame buffers per event

            //Display section
            clrScr(setting->color[COLOR_BACKGR]);

            x = Menu_start_x;
            y = Menu_start_y;

            printXY(LNG(STARTUP_SETTINGS), x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            y += FONT_HEIGHT / 2;

            sprintf(c, "  %s: %d", LNG(Number_of_CNFs), setting->numCNF);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (setting->swapKeys)
                sprintf(c, "  %s: \xFF""1:%s \xFF""0:%s", LNG(Pad_mapping), LNG(OK), LNG(CANCEL));
            else
                sprintf(c, "  %s: \xFF""0:%s \xFF""1:%s", LNG(Pad_mapping), LNG(OK), LNG(CANCEL));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->usbd_file) == 0)
                sprintf(c, "  %s: %s", LNG(USBD_IRX), LNG(DEFAULT));
            else
                sprintf(c, "  %s: %s", LNG(USBD_IRX), setting->usbd_file);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s: %d", LNG(Initial_Delay), setting->Init_Delay);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s: %d", LNG(Default_Timeout), setting->timeout);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (setting->usbkbd_used)
                sprintf(c, "  %s: %s", LNG(USB_Keyboard_Used), LNG(ON));
            else
                sprintf(c, "  %s: %s", LNG(USB_Keyboard_Used), LNG(OFF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->usbkbd_file) == 0)
                sprintf(c, "  %s: %s", LNG(USB_Keyboard_IRX), LNG(DEFAULT));
            else
                sprintf(c, "  %s: %s", LNG(USB_Keyboard_IRX), setting->usbkbd_file);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->kbdmap_file) == 0)
                sprintf(c, "  %s: %s", LNG(USB_Keyboard_Map), LNG(DEFAULT));
            else
                sprintf(c, "  %s: %s", LNG(USB_Keyboard_Map), setting->kbdmap_file);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->CNF_Path) == 0)
                sprintf(c, "  %s: %s", LNG(CNF_Path_override), LNG(NONE));
            else
                sprintf(c, "  %s: %s", LNG(CNF_Path_override), setting->CNF_Path);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->usbmass_file) == 0)
                sprintf(c, "  %s: %s", LNG(USB_Mass_IRX), LNG(DEFAULT));
            else
                sprintf(c, "  %s: %s", LNG(USB_Mass_IRX), setting->usbmass_file);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->lang_file) == 0)
                sprintf(c, "  %s: %s", LNG(Language_File), LNG(DEFAULT));
            else
                sprintf(c, "  %s: %s", LNG(Language_File), setting->lang_file);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->font_file) == 0)
                sprintf(c, "  %s: %s", LNG(Font_File), LNG(DEFAULT));
            else
                sprintf(c, "  %s: %s", LNG(Font_File), setting->font_file);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->LK_Path[15]) == 0)
                sprintf(c, "  ESR elf: %s", LNG(DEFAULT));
            else
                sprintf(c, "  ESR elf: %s", setting->LK_Path[15]);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (strlen(setting->LK_Path[16]) == 0)
                sprintf(c, "  OSDSYS kelf: %s", LNG(DEFAULT));
            else
                sprintf(c, "  OSDSYS kelf: %s", setting->LK_Path[16]);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            y += FONT_HEIGHT / 2;
            sprintf(c, "  %s", LNG(RETURN));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            //Cursor positioning section
            y = Menu_start_y + s * FONT_HEIGHT + FONT_HEIGHT / 2;

            if (s >= max_s)
                y += FONT_HEIGHT / 2;
            drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

            //Tooltip section
            if ((s == 2) || (s == 6)) {  //usbkbd_used
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(Change));
                else
                    sprintf(c, "\xFF""0:%s", LNG(Change));
            } else if ((s == 1) || (s == 4) || (s == 5)) {  //numCNF || Init_Delay || timeout
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s", LNG(Add), LNG(Subtract));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s", LNG(Add), LNG(Subtract));
            } else if ((s == 3) || (s == 7) || (s == 8) || (s == 9) || (s == 10)
                       //usbd_file||usbkbd_file||kbdmap_file||CNF_Path||usbmass_file
                       //Language||Fontfile||ESR_elf||OSDSYS_kelf
                       || (s == 11) || (s == 12) || (s == 13) || (s == 14)) {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s", LNG(Browse), LNG(Clear));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s", LNG(Browse), LNG(Clear));
            } else {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(OK));
                else
                    sprintf(c, "\xFF""0:%s", LNG(OK));
            }
            sprintf(tmp, " \xFF""3:%s", LNG(Return));
            strcat(c, tmp);
            setScrTmp("", c);
        }  //ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;

    }  //ends while
}  //ends Config_Startup
//---------------------------------------------------------------------------
// Network settings GUI by Slam-Tilt
//---------------------------------------------------------------------------
void saveNetworkSettings(char *Message)
{
    char firstline[50];
    extern char ip[16];
    extern char netmask[16];
    extern char gw[16];
    int out_fd, in_fd;
    int ret = 0, i = 0;
    int size, sizeleft = 0;
    char *ipconfigfile = 0;
    char path[MAX_PATH];

    // Default message, will get updated if save is sucessfull
    sprintf(Message, "%s", LNG(Saved_Failed));

    sprintf(firstline, "%s %s %s\n\r", ip, netmask, gw);



    // This block looks at the existing ipconfig.dat and works out if there is
    // already any data beyond the first line. If there is it will get appended to the output
    // to new file later.

    if (uLE_related(path, "uLE:/IPCONFIG.DAT") == 1)
        in_fd = genOpen(path, O_RDONLY);
    else
        in_fd = -1;

    if (strncmp(path, "mc", 2)) {
        mcSync(0, NULL, NULL);
        mcMkDir(0, 0, "SYS-CONF");
        mcSync(0, NULL, &ret);
    }

    if (in_fd >= 0) {

        size = genLseek(in_fd, 0, SEEK_END);
        printf("size of existing file is %ibytes\n\r", size);

        ipconfigfile = (char *)malloc(size);

        genLseek(in_fd, 0, SEEK_SET);
        genRead(in_fd, ipconfigfile, size);


        for (i = 0; (ipconfigfile[i] != 0 && i <= size); i++)

        {
            // printf("%i-%c\n\r",i,ipconfigfile[i]);
        }

        sizeleft = size - i;

        genClose(in_fd);
    } else
        strcpy(path, "mc0:/SYS-CONF/IPCONFIG.DAT");

    // Writing the data out

    out_fd = genOpen(path, O_WRONLY | O_TRUNC | O_CREAT);
    if (out_fd >= 0) {
        mcSync(0, NULL, &ret);
        genWrite(out_fd, firstline, strlen(firstline));
        mcSync(0, NULL, &ret);

        // If we have any extra data, spit that out too.
        if (sizeleft > 0) {
            mcSync(0, NULL, &ret);
            genWrite(out_fd, &ipconfigfile[i], sizeleft);
            mcSync(0, NULL, &ret);
        }

        sprintf(Message, "%s %s", LNG(Saved), path);

        genClose(out_fd);
    }
}
//---------------------------------------------------------------------------
// Convert IP string to numbers
//---------------------------------------------------------------------------
void ipStringToOctet(char *ip, int ip_octet[4])
{

    // This takes a string (ip) representing an IP address and converts it
    // into an array of ints (ip_octet)
    // Rewritten 22/10/05

    char oct_str[5];
    int oct_cnt, i;

    oct_cnt = 0;
    oct_str[0] = 0;

    for (i = 0; ((i <= strlen(ip)) && (oct_cnt < 4)); i++) {
        if ((ip[i] == '.') | (i == strlen(ip))) {
            ip_octet[oct_cnt] = atoi(oct_str);
            oct_cnt++;
            oct_str[0] = 0;
        } else
            sprintf(oct_str, "%s%c", oct_str, ip[i]);
    }
}
//---------------------------------------------------------------------------
data_ip_struct BuildOctets(char *ip, char *nm, char *gw)
{

    // Populate 3 arrays with the ip address (as ints)

    data_ip_struct iplist;

    ipStringToOctet(ip, iplist.ip);
    ipStringToOctet(nm, iplist.nm);
    ipStringToOctet(gw, iplist.gw);

    return (iplist);
}
//---------------------------------------------------------------------------
void Config_Network(void)
{
    // Menu System for Network Settings Page.

    int s, l;
    int x, y;
    int event, post_event = 0;
    char c[MAX_PATH];
    extern char ip[16];
    extern char netmask[16];
    extern char gw[16];
    data_ip_struct ipdata;
    char NetMsg[MAX_PATH] = "";
    char path[MAX_PATH];

    event = 1;  //event = initial entry
    s = 1;
    l = 1;
    ipdata = BuildOctets(ip, netmask, gw);

    while (1) {
        //Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_UP) {
                event |= 2;  //event |= valid pad command
                if (s != 1)
                    s--;
                else {
                    s = 5;
                    l = 1;
                }
            } else if (new_pad & PAD_DOWN) {
                event |= 2;  //event |= valid pad command
                if (s != 5)
                    s++;
                else
                    s = 1;
                if (s > 3)
                    l = 1;
            } else if (new_pad & PAD_LEFT) {
                event |= 2;  //event |= valid pad command
                if (s < 4)
                    if (l > 1)
                        l--;
            } else if (new_pad & PAD_RIGHT) {
                event |= 2;  //event |= valid pad command
                if (s < 4)
                    if (l < 5)
                        l++;
            } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;  //event |= valid pad command
                if ((s < 4) && (l > 1)) {
                    if (s == 1) {
                        if (ipdata.ip[l - 2] > 0) {
                            ipdata.ip[l - 2]--;
                        }
                    } else if (s == 2) {
                        if (ipdata.nm[l - 2] > 0) {
                            ipdata.nm[l - 2]--;
                        }
                    } else if (s == 3) {
                        if (ipdata.gw[l - 2] > 0) {
                            ipdata.gw[l - 2]--;
                        }
                    }
                }
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;  //event |= valid pad command
                if ((s < 4) && (l > 1)) {
                    if (s == 1) {
                        if (ipdata.ip[l - 2] < 255) {
                            ipdata.ip[l - 2]++;
                        }
                    } else if (s == 2) {
                        if (ipdata.nm[l - 2] < 255) {
                            ipdata.nm[l - 2]++;
                        }
                    } else if (s == 3) {
                        if (ipdata.gw[l - 2] < 255) {
                            ipdata.gw[l - 2]++;
                        }
                    }

                }

                else if (s == 4) {
                    sprintf(ip, "%i.%i.%i.%i", ipdata.ip[0], ipdata.ip[1], ipdata.ip[2], ipdata.ip[3]);
                    sprintf(netmask, "%i.%i.%i.%i", ipdata.nm[0], ipdata.nm[1], ipdata.nm[2], ipdata.nm[3]);
                    sprintf(gw, "%i.%i.%i.%i", ipdata.gw[0], ipdata.gw[1], ipdata.gw[2], ipdata.gw[3]);

                    saveNetworkSettings(NetMsg);
                } else
                    return;
            } else if (new_pad & PAD_TRIANGLE)
                return;
        }

        if (event || post_event) {  //NB: We need to update two frame buffers per event

            //Display section
            clrScr(setting->color[COLOR_BACKGR]);

            x = Menu_start_x;
            y = Menu_start_y;

            printXY(LNG(NETWORK_SETTINGS), x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            y += FONT_HEIGHT / 2;

            int len = (strlen(LNG(IP_Address)) + 5 > strlen(LNG(Netmask)) + 5) ?
                          strlen(LNG(IP_Address)) + 5 :
                          strlen(LNG(Netmask)) + 5;
            len = (len > strlen(LNG(Gateway)) + 5) ? len : strlen(LNG(Gateway)) + 5;
            sprintf(c, "%s:", LNG(IP_Address));
            printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
            sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.ip[0], ipdata.ip[1], ipdata.ip[2], ipdata.ip[3]);
            printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "%s:", LNG(Netmask));
            printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
            sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.nm[0], ipdata.nm[1], ipdata.nm[2], ipdata.nm[3]);
            printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "%s:", LNG(Gateway));
            printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
            sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.gw[0], ipdata.gw[1], ipdata.gw[2], ipdata.gw[3]);
            printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            y += FONT_HEIGHT / 2;

            if (uLE_related(path, "uLE:/IPCONFIG.DAT") != 1)
                strcpy(path, "mc0:/SYS-CONF/IPCONFIG.DAT");
            sprintf(c, "  %s \"%s\"", LNG(Save_to), path);
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            y += FONT_HEIGHT / 2;
            sprintf(c, "  %s", LNG(RETURN));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            //Cursor positioning section
            y = Menu_start_y + s * FONT_HEIGHT + FONT_HEIGHT / 2;

            if (s >= 4)
                y += FONT_HEIGHT / 2;
            if (s >= 5)
                y += FONT_HEIGHT / 2;
            if (l > 1)
                x += (len - 1) * FONT_WIDTH - 1 + (l - 2) * 6 * FONT_WIDTH;
            drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

            //Tooltip section
            if ((s < 4) && (l == 1)) {
                sprintf(c, "%s", LNG(Right_DPad_to_Edit));
            } else if (s < 4) {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s", LNG(Add), LNG(Subtract));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s", LNG(Add), LNG(Subtract));
            } else if (s == 4) {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(Save));
                else
                    sprintf(c, "\xFF""0:%s", LNG(Save));
            } else {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(OK));
                else
                    sprintf(c, "\xFF""0:%s", LNG(OK));
            }
            sprintf(tmp, " \xFF""3:%s", LNG(Return));
            strcat(c, tmp);
            setScrTmp(NetMsg, c);
        }  //ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;

    }  //ends while
}  //ends Config_Network
//---------------------------------------------------------------------------
// Configuration menu
//---------------------------------------------------------------------------
void config(char *mainMsg, char *CNF)
{
    char c[MAX_PATH];
    char title_tmp[MAX_ELF_TITLE];
    char *localMsg;
    int i;
    int s;
    int x, y;
    int event, post_event = 0;

    tmpsetting = setting;
    setting = (SETTING *)malloc(sizeof(SETTING));
    *setting = *tmpsetting;

    event = 1;  //event = initial entry
    s = 0;
    while (1) {
        //Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_UP) {
                event |= 2;  //event |= valid pad command
                if (s != 0)
                    s--;
                else
                    s = CANCEL;
            } else if (new_pad & PAD_DOWN) {
                event |= 2;  //event |= valid pad command
                if (s != CANCEL)
                    s++;
                else
                    s = 0;
            } else if (new_pad & PAD_LEFT) {
                event |= 2;  //event |= valid pad command
                if (s >= OK)
                    s = SHOW_TITLES;
                else
                    s = DEFAULT;
            } else if (new_pad & PAD_RIGHT) {
                event |= 2;  //event |= valid pad command
                if (s < SHOW_TITLES)
                    s = SHOW_TITLES;
                else if (s < OK)
                    s = OK;
            } else if ((new_pad & PAD_SQUARE) && (s < SHOW_TITLES)) {
                event |= 2;  //event |= valid pad command
                strcpy(title_tmp, setting->LK_Title[s]);
                if (keyboard(title_tmp, MAX_ELF_TITLE) >= 0)
                    strcpy(setting->LK_Title[s], title_tmp);
            } else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;  //event |= valid pad command
                if (s < SHOW_TITLES) {
                    setting->LK_Path[s][0] = 0;
                    setting->LK_Title[s][0] = 0;
                }
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
                event |= 2;  //event |= valid pad command
                if (s < SHOW_TITLES) {
                    getFilePath(setting->LK_Path[s], TRUE);
                    if (!strncmp(setting->LK_Path[s], "mc0", 3) ||
                        !strncmp(setting->LK_Path[s], "mc1", 3)) {
                        sprintf(c, "mc%s", &setting->LK_Path[s][3]);
                        strcpy(setting->LK_Path[s], c);
                    }
                } else if (s == SHOW_TITLES)
                    setting->Show_Titles = !setting->Show_Titles;
                else if (s == FILENAME)
                    setting->Hide_Paths = !setting->Hide_Paths;
                else if (s == SCREEN)
                    Config_Screen();
                else if (s == SETTINGS)
                    Config_Startup();
                else if (s == NETWORK)
                    Config_Network();
                else if (s == OK) {
                    free(tmpsetting);
                    saveConfig(mainMsg, CNF);
                    if (setting->GUI_skin[0]) {
                        GUI_active = 1;
                        loadSkin(BACKGROUND_PIC, 0, 0);
                    }
                    break;
                } else if (s == CANCEL)
                    goto cancel_exit;
            } else if (new_pad & PAD_TRIANGLE) {
            cancel_exit:
                free(setting);
                setting = tmpsetting;
                updateScreenMode();
                if (setting->GUI_skin[0])
                    GUI_active = 1;
                loadSkin(BACKGROUND_PIC, 0, 0);
                Load_External_Language();
                loadFont(setting->font_file);
                mainMsg[0] = 0;
                break;
            }
        }  //end if(readpad())

        if (event || post_event) {  //NB: We need to update two frame buffers per event

            //Display section
            clrScr(setting->color[COLOR_BACKGR]);

            if (s < SHOW_TITLES)
                localMsg = setting->LK_Title[s];
            else
                localMsg = "";

            x = Menu_start_x;
            y = Menu_start_y;
            printXY(LNG(Button_Settings), x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            for (i = 0; i < 12; i++) {
                switch (i) {
                    case 0:
                        strcpy(c, "  Default: ");
                        break;
                    case 1:
                        strcpy(c, "  \xFF""0     : ");
                        break;
                    case 2:
                        strcpy(c, "  \xFF""1     : ");
                        break;
                    case 3:
                        strcpy(c, "  \xFF""2     : ");
                        break;
                    case 4:
                        strcpy(c, "  \xFF""3     : ");
                        break;
                    case 5:
                        strcpy(c, "  L1     : ");
                        break;
                    case 6:
                        strcpy(c, "  R1     : ");
                        break;
                    case 7:
                        strcpy(c, "  L2     : ");
                        break;
                    case 8:
                        strcpy(c, "  R2     : ");
                        break;
                    case 9:
                        strcpy(c, "  L3     : ");
                        break;
                    case 10:
                        strcpy(c, "  R3     : ");
                        break;
                    case 11:
                        strcpy(c, "  START  : ");
                        break;
                }
                strcat(c, setting->LK_Path[i]);
                printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
                y += FONT_HEIGHT;
            }

            y += FONT_HEIGHT / 2;

            if (setting->Show_Titles)
                sprintf(c, "  %s: %s", LNG(Show_launch_titles), LNG(ON));
            else
                sprintf(c, "  %s: %s", LNG(Show_launch_titles), LNG(OFF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            if (setting->Hide_Paths)
                sprintf(c, "  %s: %s", LNG(Hide_full_ELF_paths), LNG(ON));
            else
                sprintf(c, "  %s: %s", LNG(Hide_full_ELF_paths), LNG(OFF));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s...", LNG(Screen_Settings));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(c, "  %s...", LNG(Startup_Settings));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(c, "  %s...", LNG(Network_Settings));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;

            sprintf(c, "  %s", LNG(OK));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(c, "  %s", LNG(Cancel));
            printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);

            //Cursor positioning section
            y = Menu_start_y + (s + 1) * FONT_HEIGHT;
            if (s >= SHOW_TITLES)
                y += FONT_HEIGHT / 2;
            drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

            //Tooltip section
            if (s < SHOW_TITLES) {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s \xFF""0:%s \xFF""2:%s", LNG(Browse), LNG(Clear), LNG(Edit_Title));
                else
                    sprintf(c, "\xFF""0:%s \xFF""1:%s \xFF""2:%s", LNG(Browse), LNG(Clear), LNG(Edit_Title));
            } else if ((s == SHOW_TITLES) || (s == FILENAME)) {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(Change));
                else
                    sprintf(c, "\xFF""0:%s", LNG(Change));
            } else {
                if (swapKeys)
                    sprintf(c, "\xFF""1:%s", LNG(OK));
                else
                    sprintf(c, "\xFF""0:%s", LNG(OK));
            }
            sprintf(tmp, " \xFF""3:%s", LNG(Return));
            strcat(c, tmp);
            setScrTmp(localMsg, c);
        }  //ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;

    }  //ends while
    loadCdModules();
}  //ends config
//---------------------------------------------------------------------------
// End of file: config.c
//---------------------------------------------------------------------------
