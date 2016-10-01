//--------------------------------------------------------------
//File name:   jpgviewer.c
//--------------------------------------------------------------
#include "launchelf.h"

static char msg0[MAX_PATH], msg1[MAX_PATH], jpgpath[MAX_PATH];
static int SlideShowTime = 5, SlideShowTrans = 2, SlideShowBegin, SlideShowStart, SlideShowStop, SlideShowSkip;
static int Brightness, TimeRemain, PrintLen;
static float PanPosX, PanPosY, PanPosX1, PanPosY1;
static float PanZoom, PanOffsetX, PanOffsetY;
float PicW, PicH, PicCoeff;
float PicWidth, PicHeight;
int PicRotate, FullScreen;

#define OFF 1
#define ZOOM 2
#define FADE 3
#define ZOOM_FADE 4

#define LIST 1
#define THUMBNAIL 2

#define NOTLOADED -1
#define BADJPG 0
#define LOADED 1

//--------------------------------------------------------------
static void Command_List(void)
{
    int x, y;
    int event, post_event = 0;

    int command_len = strlen(LNG(Start_StartStop_Slideshow)) > strlen(LNG(Start_StartStop_Slideshow)) ?
                          strlen(LNG(Start_StartStop_Slideshow)) :
                          strlen(LNG(Start_StartStop_Slideshow));
    command_len = strlen(LNG(L1R1_Slideshow_Timer)) > command_len ?
                      strlen(LNG(L1R1_Slideshow_Timer)) :
                      command_len;
    command_len = strlen(LNG(L2R2_Slideshow_Transition)) > command_len ?
                      strlen(LNG(L2R2_Slideshow_Transition)) :
                      command_len;
    command_len = strlen(LNG(LeftRight_Pad_PrevNext_Picture)) > command_len ?
                      strlen(LNG(LeftRight_Pad_PrevNext_Picture)) :
                      command_len;
    command_len = strlen(LNG(UpDown_Pad_Rotate_Picture)) > command_len ?
                      strlen(LNG(UpDown_Pad_Rotate_Picture)) :
                      command_len;
    command_len = strlen(LNG(Left_Joystick_Panorama)) > command_len ?
                      strlen(LNG(Left_Joystick_Panorama)) :
                      command_len;
    command_len = strlen(LNG(Right_Joystick_Vertical_Zoom)) > command_len ?
                      strlen(LNG(Right_Joystick_Vertical_Zoom)) :
                      command_len;
    command_len = strlen(LNG(FullScreen_Mode)) + 3 > command_len ?
                      strlen(LNG(FullScreen_Mode)) + 3 :
                      command_len;
    command_len = strlen(LNG(Exit_To_Jpg_Browser)) + 3 > command_len ?
                      strlen(LNG(Exit_To_Jpg_Browser)) + 3 :
                      command_len;

    int Command_ch_w = command_len + 1;                                           //Total characters in longest Command Name.
    int Command_ch_h = 9;                                                         //Total Command lines number.
    int cSprite_Y1 = SCREEN_HEIGHT / 2 - ((Command_ch_h + 1) * FONT_HEIGHT) / 2;  //Top edge
    int cSprite_X2 = SCREEN_WIDTH / 2 + ((Command_ch_w + 3) * FONT_WIDTH) / 2;    //Right edge
    int cSprite_X1 = cSprite_X2 - (Command_ch_w + 3) * FONT_WIDTH - 4;            //Left edge
    int cSprite_Y2 = cSprite_Y1 + (Command_ch_h + 1) * FONT_HEIGHT;               //Bottom edge

    char tmp[MAX_PATH];

    event = 1;  //event = initial entry.
    while (1) {
        //Pad response section.
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad) {
                event |= 2;  //event |= valid pad command.
                break;
            }
        }

        if (event || post_event) {  //NB: We need to update two frame buffers per event.

            //Display section.
            drawOpSprite(setting->color[0], cSprite_X1, cSprite_Y1, cSprite_X2, cSprite_Y2);
            drawFrame(cSprite_X1, cSprite_Y1, cSprite_X2, cSprite_Y2, setting->color[1]);

            y = cSprite_Y1 + FONT_HEIGHT / 2;
            x = cSprite_X1 + 2 * FONT_WIDTH;

            printXY(LNG(Start_StartStop_Slideshow), x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            printXY(LNG(L1R1_Slideshow_Timer), x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            printXY(LNG(L2R2_Slideshow_Transition), x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            printXY(LNG(LeftRight_Pad_PrevNext_Picture), x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            printXY(LNG(UpDown_Pad_Rotate_Picture), x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            printXY(LNG(Left_Joystick_Panorama), x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            printXY(LNG(Right_Joystick_Vertical_Zoom), x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            if (swapKeys)
                sprintf(tmp, "ÿ1: %s", LNG(FullScreen_Mode));
            else
                sprintf(tmp, "ÿ0: %s", LNG(FullScreen_Mode));
            printXY(tmp, x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;
            sprintf(tmp, "ÿ3: %s", LNG(Exit_To_Jpg_Browser));
            printXY(tmp, x, y, setting->color[3], TRUE, 0);
            y += FONT_HEIGHT;

        }  //ends if(event||post_event).
        drawScr();
        post_event = event;
        event = 0;
    }  //ends while.
}  //ends Command_List.
//--------------------------------------------------------------
static void View_Render(void)
{

    char *name, tmp[MAX_PATH];

    float ScreenPosX, ScreenPosX1, ScreenPosY, ScreenPosY1;
    float ScreenOffsetX, ScreenOffsetY;
    float TmpPosX, TmpPosY;

    // Init picture position on screen
    if (FullScreen) {
        ScreenPosX = 0.0f;
        ScreenPosX1 = SCREEN_WIDTH;
        ScreenPosY = 0.0f;
        ScreenPosY1 = SCREEN_HEIGHT;
    } else {
        ScreenPosX = SCREEN_MARGIN;
        ScreenPosX1 = SCREEN_WIDTH - SCREEN_MARGIN;
        ScreenPosY = Frame_start_y;
        ScreenPosY1 = Frame_end_y;
    }

    // Init black bars
    if ((ScreenOffsetX = ((ScreenPosX1 - ScreenPosX) - ((ScreenPosY1 - ScreenPosY) * PicCoeff)) / 2) <= 0.0f)
        ScreenOffsetX = 0.0f;
    if ((ScreenOffsetY = ((ScreenPosY1 - ScreenPosY) - ((ScreenPosX1 - ScreenPosX) / PicCoeff)) / 2) <= 0.0f)
        ScreenOffsetY = 0.0f;

    // Init panorama
    PanPosX = 0.0f;
    PanPosY = 0.0f;
    PanPosX1 = PicWidth;
    PanPosY1 = PicHeight;
    if (PanZoom == 1.0f) {
        PanOffsetX = 0.0f;
        PanOffsetY = 0.0f;
    } else {
        PanPosX = TmpPosX = ((PicWidth / 8) - ((1.5f - PanZoom) * (PicWidth / 4)));
        if ((PanPosX += PanOffsetX * TmpPosX) <= 0.0f)
            PanPosX = 0.0f;
        if ((PanPosX1 = PicWidth - (TmpPosX * 2 - PanPosX)) >= PicWidth)
            PanPosX1 = PicWidth;
        PanPosY = TmpPosY = ((PicHeight / 8) - ((1.5f - PanZoom) * (PicHeight / 4)));
        if ((PanPosY += PanOffsetY * TmpPosY) <= 0.0f)
            PanPosY = 0.0f;
        if ((PanPosY1 = PicHeight - (TmpPosY * 2 - PanPosY)) >= PicHeight)
            PanPosY1 = PicHeight;
    }

    // Clear screen
    clrScr(setting->color[0]);

    // Draw color8 graph4
    gsKit_prim_sprite(gsGlobal, ScreenPosX, ScreenPosY, ScreenPosX1, ScreenPosY1, 0, setting->color[7]);
    // Draw picture
    if (PicRotate == 0 || PicRotate == 1 || PicRotate == 3) {  // No rotation, rotate +90°, -90°
        gsKit_prim_sprite_texture(gsGlobal,
                                  &TexPicture,
                                  ScreenPosX + ScreenOffsetX, ScreenPosY + ScreenOffsetY, PanPosX, PanPosY,
                                  ScreenPosX1 - ScreenOffsetX, ScreenPosY1 - ScreenOffsetY, PanPosX1, PanPosY1,
                                  0, GS_SETREG_RGBAQ(Brightness * 1.28f, Brightness * 1.28f, Brightness * 1.28f, 0x80, 0x00));
    } else if (PicRotate == 2) {  // Rotate 180°
        gsKit_prim_sprite_texture(gsGlobal,
                                  &TexPicture,
                                  ScreenPosX + ScreenOffsetX, ScreenPosY + ScreenOffsetY, PanPosX1, PanPosY1,
                                  ScreenPosX1 - ScreenOffsetX, ScreenPosY1 - ScreenOffsetY, PanPosX, PanPosY,
                                  0, GS_SETREG_RGBAQ(Brightness * 1.28f, Brightness * 1.28f, Brightness * 1.28f, 0x80, 0x00));
    }
    setBrightness(50);

    if (!FullScreen) {
        //Tooltip section
        strcpy(tmp, jpgpath);
        name = strrchr(tmp, '/');
        strcpy(name, name + 1);
        msg0[0] = '\0';
        sprintf(msg0, "%s  %s: %s  %s: %d*%d ",
                LNG(Jpg_Viewer), LNG(Picture), name, LNG(Size), (int)PicW, (int)PicH);
        msg1[0] = '\0';
        sprintf(msg1, "Select: %s  ", LNG(Command_List));
        tmp[0] = '\0';
        if (TimeRemain < 60)
            sprintf(tmp, "%s: %d sec  %s: ", LNG(Timer), TimeRemain, LNG(Transition));
        else
            sprintf(tmp, "%s: %d m %d sec  %s: ", LNG(Timer), TimeRemain / 60, TimeRemain % 60, LNG(Transition));
        strcat(msg1, tmp);
        if (SlideShowTrans == OFF)
            strcat(msg1, LNG(Off));
        else if (SlideShowTrans == ZOOM)
            strcat(msg1, LNG(Zoom));
        else if (SlideShowTrans == FADE)
            strcat(msg1, LNG(Fade));
        else if (SlideShowTrans == ZOOM_FADE)
            strcat(msg1, LNG(ZoomFade));
        setScrTmp(msg0, msg1);
    } /* end FullScreen */
    drawScr();
} /* end View_Render */

//--------------------------------------------------------------
static void View_Input(void)
{

    int i = 0;
    u64 OldTime = Timer() + 1000;

    while (1) {

        if (SlideShowStart) {
            if (Timer() >= OldTime) {
                OldTime = Timer() + 1000;
                if (--TimeRemain <= 0)
                    break;
                View_Render();
            }
        } else {
            if (Timer() >= OldTime) {
                OldTime = Timer() + 1000;
                TimeRemain = SlideShowTime;
                View_Render();
            }
        }

        //Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad & PAD_R3_V0) {  // PanZoom In
                if (PanZoom < 2.75f) {
                    PanZoom += 0.01f * joy_value / 32;
                    SlideShowStart = 0;
                    View_Render();
                }                              /* end if */
            } else if (new_pad & PAD_R3_V1) {  // PanZoom Out
                if (PanZoom > 1.0f) {
                    PanZoom -= 0.01f * joy_value / 32;
                    SlideShowStart = 0;
                    View_Render();
                }                              /* end if */
            } else if (new_pad & PAD_L3_H1) {  // Move Right
                if (PanOffsetX < 0.95f) {
                    PanOffsetX += 0.05f / PanZoom * joy_value / 32;
                    SlideShowStart = 0;
                    View_Render();
                }                              /* end if */
            } else if (new_pad & PAD_L3_H0) {  // Move Left
                if (PanOffsetX > -0.95f) {
                    PanOffsetX -= 0.05f / PanZoom * joy_value / 32;
                    SlideShowStart = 0;
                    View_Render();
                }                              /* end if */
            } else if (new_pad & PAD_L3_V1) {  // Move Down
                if (PanOffsetY < 0.95f) {
                    PanOffsetY += 0.05f / PanZoom * joy_value / 32;
                    SlideShowStart = 0;
                    View_Render();
                }                              /* end if */
            } else if (new_pad & PAD_L3_V0) {  // Move Up
                if (PanOffsetY > -0.95f) {
                    PanOffsetY -= 0.05f / PanZoom * joy_value / 32;
                    SlideShowStart = 0;
                    View_Render();
                }                              /* end if */
            } else if (new_pad & PAD_RIGHT) {  // Next Pic
                SlideShowSkip = 1;
                break;
            } else if (new_pad & PAD_LEFT) {  // Prev Pic
                SlideShowSkip = -1;
                break;
            } else if (new_pad & PAD_UP) {  // Rotate Pic +
                if (PanZoom != 1.0f) {
                    for (i = 0; i < 35; ++i) {
                        if ((PanZoom -= 0.05F) <= 1.0F)
                            PanZoom = 1.0F;
                        View_Render();
                    } /* end for */
                }     /* end if */
                if (++PicRotate >= 4)
                    PicRotate = 0;
                loadSkin(JPG_PIC, jpgpath, 0);
                WaitTime = Timer();
                while (Timer() < WaitTime + 500)
                    ;  // Wait To Ensure Switch
                View_Render();
                TimeRemain = SlideShowTime;
            } else if (new_pad & PAD_DOWN) {  // Rotate Pic -
                if (PanZoom != 1.0f) {
                    for (i = 0; i < 35; ++i) {
                        if ((PanZoom -= 0.05F) <= 1.0F)
                            PanZoom = 1.0F;
                        View_Render();
                    } /* end for */
                }     /* end if */
                if (--PicRotate <= -1)
                    PicRotate = 3;
                loadSkin(JPG_PIC, jpgpath, 0);
                WaitTime = Timer();
                while (Timer() < WaitTime + 500)
                    ;  // Wait To Ensure Switch
                View_Render();
                TimeRemain = SlideShowTime;
            } else if (new_pad & PAD_R1) {
                if (++SlideShowTime >= 3600)
                    SlideShowTime = 3600;
                WaitTime = Timer();
                while (Timer() < WaitTime + 100)
                    ;  // Wait To Ensure Switch
                if (++TimeRemain >= 3600)
                    TimeRemain = 3600;
                View_Render();
            } else if (new_pad & PAD_L1) {
                if (--SlideShowTime <= 1)
                    SlideShowTime = 1;
                WaitTime = Timer();
                while (Timer() < WaitTime + 100)
                    ;  // Wait To Ensure Switch
                if (--TimeRemain <= 1)
                    TimeRemain = 1;
                View_Render();
            } else if (new_pad & PAD_R2) {
                if (++SlideShowTrans > 4)
                    SlideShowTrans = 1;
                WaitTime = Timer();
                while (Timer() < WaitTime + 500)
                    ;  // Wait To Ensure Switch
                View_Render();
            } else if (new_pad & PAD_L2) {
                if (--SlideShowTrans < 1)
                    SlideShowTrans = 4;
                WaitTime = Timer();
                while (Timer() < WaitTime + 500)
                    ;  // Wait To Ensure Switch
                View_Render();
            } else if (new_pad & PAD_TRIANGLE) {  // Stop SlideShow
                SlideShowStop = 1;
                break;
            } else if (new_pad & PAD_START) {  // Play/Pause SlideShow
                if (PanZoom != 1.0f) {
                    for (i = 0; i < 35; ++i) {
                        if ((PanZoom -= 0.05F) <= 1.0F)
                            PanZoom = 1.0F;
                        View_Render();
                    } /* end for */
                }     /* end if */
                SlideShowStart = !SlideShowStart;
                WaitTime = Timer();
                while (Timer() < WaitTime + 500)
                    ;  // Wait To Ensure Switch
                TimeRemain = SlideShowTime;
                View_Render();
            } else if (new_pad & PAD_SELECT) {  // Command List
                Command_List();
                View_Render();
                WaitTime = Timer();
                while (Timer() < WaitTime + 500)
                    ;  // Wait To Ensure Switch
                OldTime = Timer() + 1000;
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  // Full Screen
                if (PanZoom != 1.0f) {
                    for (i = 0; i < 35; ++i) {
                        if ((PanZoom -= 0.05F) <= 1.0F)
                            PanZoom = 1.0F;
                        View_Render();
                    } /* end for */
                }     /* end if */
                FullScreen = !FullScreen;
                if (FullScreen) {
                    loadSkin(JPG_PIC, jpgpath, 0);
                } else {
                    loadSkin(BACKGROUND_PIC, 0, 0);
                    loadSkin(JPG_PIC, jpgpath, 0);
                }
                WaitTime = Timer();
                while (Timer() < WaitTime + 500)
                    ;  // Wait To Ensure Switch
                View_Render();
            } /* end else */
        }     //ends pad response section
    }         //ends while
} /* end View_Input */

//--------------------------------------------------------------
static void loadPic(void)
{
    int i = 0;

    loadSkin(JPG_PIC, jpgpath, 0);

    Brightness = 0;
    PanZoom = 3.0f;
    PanOffsetX = 0.0f;
    PanOffsetY = 0.0f;

    if (testjpg) {
        switch (SlideShowTrans) {
            case OFF: {
                View_Render();
            } break;
            case ZOOM: {
                Brightness = 100;
                for (i = 0; i < 100; ++i) {
                    if ((PanZoom -= 0.02F) <= 1.0F)
                        PanZoom = 1.0F;
                    View_Render();
                } /* end for */
            } break;
            case FADE: {
                PanZoom = 1.0f;
                for (i = 0; i < 100; ++i) {
                    ++Brightness;
                    View_Render();
                } /* end for */
            } break;
            case ZOOM_FADE: {
                for (i = 0; i < 100; ++i) {
                    if ((PanZoom -= 0.02F) <= 1.0F)
                        PanZoom = 1.0F;
                    ++Brightness;
                    View_Render();
                } /* end for */
            } break;
        } /* end switch */

        Brightness = 100;
        PanZoom = 1.0f;
        PanOffsetX = 0.0f;
        PanOffsetY = 0.0f;
        TimeRemain = SlideShowTime;

        View_Render();
        View_Input();

        switch (SlideShowTrans) {
            case OFF: {
                View_Render();
            } break;
            case ZOOM: {
                for (i = 0; i < 100; ++i) {
                    if ((PanZoom += 0.02F) >= 3.0F)
                        PanZoom = 3.0F;
                    View_Render();
                } /* end for */
            } break;
            case FADE: {
                for (i = 0; i < 100; ++i) {
                    --Brightness;
                    View_Render();
                } /* end for */
            } break;
            case ZOOM_FADE: {
                for (i = 0; i < 100; ++i) {
                    if ((PanZoom += 0.02F) >= 3.0F)
                        PanZoom = 3.0F;
                    --Brightness;
                    View_Render();
                } /* end for */
            } break;
        } /* end switch */
    }     /* end testjpg */
} /* end loadPic */

//--------------------------------------------------------------
void JpgViewer(void)
{
    char path[MAX_PATH],
        cursorEntry[MAX_PATH], ext[8],
        tmp[MAX_PATH], tmp1[MAX_PATH], *p;
    u64 color;
    FILEINFO files[MAX_ENTRY];
    int thumb_test[MAX_ENTRY];
    int jpg_browser_cd, jpg_browser_up, jpg_browser_repos, jpg_browser_pushed;
    int thumb_load;
    int jpg_browser_sel, jpg_browser_nfiles, old_jpg_browser_sel;
    int top = 0, test_top = 0, rows = 0, rows_down = 0;
    int x = 0, y = 0, y0 = 0, y1 = 0;
    int thumb_num = 0, thumb_top = 0;
    int jpg_browser_mode = LIST;
    int print_name = 0;
    int Len = 0;
    int event, post_event = 0;
    int i, t = 0;
    int CountJpg = 0;

    jpg_browser_cd = TRUE;
    jpg_browser_up = FALSE;
    jpg_browser_repos = FALSE;
    jpg_browser_pushed = TRUE;
    thumb_load = TRUE;
    jpg_browser_sel = 0;
    jpg_browser_nfiles = 0;
    old_jpg_browser_sel = 0;

    SlideShowTime = setting->JpgView_Timer;
    SlideShowTrans = setting->JpgView_Trans;
    FullScreen = setting->JpgView_Full;
    SlideShowBegin = SlideShowStart = SlideShowStop = SlideShowSkip = 0;
    PicRotate = PrintLen = 0;

    for (i = 0; i < MAX_ENTRY; ++i)
        thumb_test[i] = NOTLOADED;

    strcpy(ext, "jpg");

    mountedParty[0][0] = 0;
    mountedParty[1][0] = 0;

    path[0] = '\0';
    jpgpath[0] = '\0';

    if (jpg_browser_mode == LIST) {
        rows = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;
    } else {
        if (TV_mode == TV_mode_NTSC)
            rows = 4;
        else if (TV_mode == TV_mode_PAL)
            rows = 5;
    }

    loadIcon();

    event = 1;  //event = initial entry
    while (1) {

        //Pad response section
        waitPadReady(0, 0);
        if (readpad()) {
            if (new_pad) {
                jpg_browser_pushed = TRUE;
                print_name = 0;
                event |= 2;  //event |= pad command
                tmp[0] = 0;
                tmp1[0] = 0;
                t = 0;
            }
            if (new_pad & PAD_UP)
                jpg_browser_sel--;
            else if (new_pad & PAD_DOWN)
                jpg_browser_sel++;
            else if (new_pad & PAD_LEFT)
                jpg_browser_sel -= rows - 1;
            else if (new_pad & PAD_RIGHT) {
                rows_down = 1;
                old_jpg_browser_sel = jpg_browser_sel + rows - 1;
                jpg_browser_sel++;
            } else if (new_pad & PAD_TRIANGLE) {
                jpg_browser_up = TRUE;
                thumb_load = TRUE;
            } else if (new_pad & PAD_SQUARE) {
                if (jpg_browser_mode == LIST) {
                    jpg_browser_mode = THUMBNAIL;
                    jpg_browser_sel = 0;
                    thumb_load = TRUE;
                } else
                    jpg_browser_mode = LIST;
            } else if (new_pad & PAD_R1) {
                if (++SlideShowTime >= 300)
                    SlideShowTime = 300;
            } else if (new_pad & PAD_L1) {
                if (--SlideShowTime <= 1)
                    SlideShowTime = 1;
            } else if (new_pad & PAD_R2) {
                char *temp = PathPad_menu(path);

                if (temp != NULL) {
                    strcpy(path, temp);
                    jpg_browser_cd = TRUE;
                    thumb_load = TRUE;
                }
            } else if (new_pad & PAD_L2) {
                if (--SlideShowTrans < 1)
                    SlideShowTrans = 4;
            } else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {  //Pushed OK
                if (files[jpg_browser_sel].stats.attrFile & MC_ATTR_SUBDIR) {
                    //pushed OK for a folder
                    thumb_load = TRUE;
                    if (!strcmp(files[jpg_browser_sel].name, "..")) {
                        jpg_browser_up = TRUE;
                    } else {
                        strcat(path, files[jpg_browser_sel].name);
                        strcat(path, "/");
                        jpg_browser_cd = TRUE;
                    }
                } else {
                //pushed OK for a file
                restart:
                    sprintf(jpgpath, "%s%s", path, files[jpg_browser_sel].name);

                    SlideShowBegin = 1;

                    if (!SlideShowStart)
                        goto single;
                repeat:
                    for (i = 0; i < jpg_browser_nfiles; ++i) {
                        if (SlideShowBegin) {
                            i = jpg_browser_sel + 1;
                            SlideShowBegin = 0;
                        }
                        sprintf(jpgpath, "%s%s", path, files[i].name);
                        loadPic();
                        PicRotate = 0;
                        if (testjpg)
                            ++CountJpg;
                        if (SlideShowSkip == 1)
                            SlideShowSkip = 0;
                        else if (SlideShowSkip == -1) {
                            i -= 2;  //Loop index back to JPG before previous
                            if (i <= 0)
                                i += jpg_browser_nfiles - 1;  //Loop index back to JPG before final
                            SlideShowSkip = 0;
                        }
                        if (SlideShowStop)
                            goto end;
                    } /* end for */
                    goto end;
                single:
                    loadPic();
                    PicRotate = 0;
                    if (testjpg)
                        ++CountJpg;
                    if (SlideShowSkip == 1 || !testjpg) {
                        if (++jpg_browser_sel > jpg_browser_nfiles)
                            jpg_browser_sel = 0;
                        SlideShowSkip = 0;
                        goto restart;
                    } else if (SlideShowSkip == -1) {
                        jpg_browser_sel -= 1;
                        if (jpg_browser_sel <= 0)
                            jpg_browser_sel += jpg_browser_nfiles - 1;  //Go back to final JPG
                        SlideShowSkip = 0;
                        goto restart;
                    }
                end:
                    if (SlideShowStart && !SlideShowStop && CountJpg > 0)
                        goto repeat;
                    if (FullScreen) {
                        loadSkin(BACKGROUND_PIC, 0, 0);
                        loadIcon();
                    }
                    SlideShowStart = SlideShowStop = CountJpg = PicRotate = 0;
                    thumb_load = TRUE;
                } /* end else file */
            } else if (new_pad & PAD_SELECT) {
                if (mountedParty[0][0] != 0) {
                    fileXioUmount("pfs0:");
                    mountedParty[0][0] = 0;
                }
                if (mountedParty[1][0] != 0) {
                    fileXioUmount("pfs1:");
                    mountedParty[1][0] = 0;
                }
                setting->JpgView_Timer = SlideShowTime;
                setting->JpgView_Trans = SlideShowTrans;
                setting->JpgView_Full = FullScreen;
                return;
            } else if (new_pad & PAD_START) {
                if (path[0] != 0) {
                    SlideShowStart = 1;
                    SlideShowBegin = 0;
                    goto repeat;
                }
            }
        }  //ends pad response section

        //Thumb init
        if (thumb_load) {
            jpg_browser_sel = 0;
            gsGlobal->CurrentPointer = 0x288000;
            for (i = 0; i < MAX_ENTRY; ++i)
                thumb_test[i] = NOTLOADED;
        }

        //browser path adjustment section
        if (jpg_browser_up) {
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
            jpg_browser_cd = TRUE;
            jpg_browser_repos = TRUE;
        }  //ends 'if(jpg_browser_up)'
        //----- Process newly entered directory here (incl initial entry)
        if (jpg_browser_cd) {
            jpg_browser_nfiles = setFileList(path, ext, files, JPG_CNF);
            jpg_browser_sel = 0;
            top = 0;
            if (jpg_browser_repos) {
                jpg_browser_repos = FALSE;
                for (i = 0; i < jpg_browser_nfiles; i++) {
                    if (!strcmp(cursorEntry, files[i].name)) {
                        jpg_browser_sel = i;
                        top = jpg_browser_sel - 3;
                        break;
                    }
                }
            }  //ends if(jpg_browser_repos)
            jpg_browser_cd = FALSE;
            jpg_browser_up = FALSE;
        }  //ends if(jpg_browser_cd)
        if (setting->discControl && !strncmp(path, "cdfs", 4)) {
            CDVD_Stop();
            sceCdSync(0);
        }
        if (top > jpg_browser_nfiles - rows)
            top = jpg_browser_nfiles - rows;
        if (top < 0)
            top = 0;
        if (jpg_browser_sel >= jpg_browser_nfiles)
            jpg_browser_sel = jpg_browser_nfiles - 1;
        if (jpg_browser_sel < 0)
            jpg_browser_sel = 0;
        if (jpg_browser_sel >= top + rows)
            top = jpg_browser_sel - rows + 1;
        if (jpg_browser_sel < top)
            top = jpg_browser_sel;

        t++;

        if (t & 0x0F)
            event |= 4;  //repetitive timer event

        if (event || post_event) {  //NB: We need to update two frame buffers per event

            //Display section
            clrScr(setting->color[0]);

            if (jpg_browser_mode == LIST) {
                x = Menu_start_x;
                y = Menu_start_y;
                rows = (Menu_end_y - Menu_start_y) / FONT_HEIGHT;
                if (rows_down == 1) {
                    jpg_browser_sel = old_jpg_browser_sel;
                    rows_down = 0;
                    old_jpg_browser_sel = 0;
                } /* end if rows_down */
                goto list;
            } else {
                x = Menu_start_x + 13;
                if (TV_mode == TV_mode_NTSC) {
                    y = Menu_start_y + 15;
                    rows = 4;
                } else if (TV_mode == TV_mode_PAL) {
                    y = Menu_start_y + 11;
                    rows = 5;
                }
            }

            if (test_top != top) {
                if (top > test_top) {
                down:
                    thumb_top = thumb_num = 0;
                    for (i = 0; i < jpg_browser_sel; ++i) {
                        if (i < top && thumb_test[i] == LOADED)
                            ++thumb_top;
                        if (i >= top && thumb_test[i] == LOADED)
                            ++thumb_num;
                    } /* end for */
                    if (thumb_test[jpg_browser_sel] == NOTLOADED && !(files[jpg_browser_sel].stats.attrFile & MC_ATTR_SUBDIR)) {
                        sprintf(jpgpath, "%s%s", path, files[jpg_browser_sel].name);
                        loadSkin(THUMB_PIC, jpgpath, thumb_top + thumb_num);
                        thumb_test[jpg_browser_sel] = testthumb;
                    } /* end if notloaded */
                    if (rows_down == 1) {
                        if (++jpg_browser_sel >= old_jpg_browser_sel) {
                            rows_down = 0;
                            old_jpg_browser_sel = 0;
                        } else
                            goto down;
                    } /* end if rows_down */
                } else {
                    thumb_top = 0;
                    for (i = 0; i < top; ++i) {
                        if (thumb_test[i] == LOADED)
                            ++thumb_top;
                    } /* end for */
                }     /* end else test_top */
                test_top = top;
            } else {
                if (rows_down == 1) {
                    jpg_browser_sel = old_jpg_browser_sel;
                    rows_down = 0;
                    old_jpg_browser_sel = 0;
                } /* end if rows_down */
            }     /* end else test_top!=top */

            thumb_num = 0;

        list:
            for (i = 0; i < rows; i++) {
                int name_limit = 0;  //Assume that no name length problems exist

                if (top + i >= jpg_browser_nfiles)
                    break;
                if (top + i == jpg_browser_sel)
                    color = setting->color[2];  //Highlight cursor line
                else
                    color = setting->color[3];

                if (!strcmp(files[top + i].name, ".."))
                    strcpy(tmp, "..");
                else {
                    strcpy(tmp, files[top + i].name);
                    name_limit = 71 * 8;
                }

                if (jpg_browser_mode == LIST) {  //List mode

                    if (name_limit) {                   //Do we need to check name length ?
                        int name_end = name_limit / 7;  //Max string length for acceptable spacing

                        if (files[top + i].stats.attrFile & MC_ATTR_SUBDIR)
                            name_end -= 1;             //For folders, reserve one character for final '/'
                        if (strlen(tmp) > name_end) {  //Is name too long for clean display ?
                            tmp[name_end - 1] = '~';   //indicate filename abbreviation
                            tmp[name_end] = 0;         //abbreviate name length to make room for details
                        }
                    }
                    if (files[top + i].stats.attrFile & MC_ATTR_SUBDIR)
                        strcat(tmp, "/");
                    printXY(tmp, x + 4, y, color, TRUE, name_limit);
                    y += FONT_HEIGHT;

                } else {  //Thumbnail mode

                    if (files[top + i].stats.attrFile & MC_ATTR_SUBDIR) {
                        strcat(tmp, "/");
                        gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
                        gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
                        gsKit_set_test(gsGlobal, GS_ATEST_OFF);
                        gsKit_prim_sprite_texture(gsGlobal,
                                                  &TexIcon[0], x + 20, (y + 10), 0, 0, x + 72 - 20, (y + 55 - 10), 16, 16,
                                                  0, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00));
                        gsKit_set_test(gsGlobal, GS_ATEST_ON);
                        gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
                        gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
                        thumb_test[top + i] = BADJPG;
                        goto frame;
                    }

                    if (thumb_load) {
                        sprintf(jpgpath, "%s%s", path, files[top + i].name);
                        loadSkin(THUMB_PIC, jpgpath, thumb_num + thumb_top);
                        thumb_test[top + i] = testthumb;
                    }

                    if (thumb_test[top + i] == LOADED) {
                        gsKit_prim_sprite_texture(gsGlobal,
                                                  &TexThumb[thumb_num + thumb_top], x, y, 0, 0, x + 72, (y + 55), 64, 32,
                                                  0, BrightColor);
                        ++thumb_num;
                    } else {  // BADJPG
                        gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
                        gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
                        gsKit_set_test(gsGlobal, GS_ATEST_OFF);
                        gsKit_prim_sprite_texture(gsGlobal,
                                                  &TexIcon[0], x + 20, (y + 10), 0, 0, x + 72 - 20, (y + 55 - 10), 16, 16,
                                                  0, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00));
                        gsKit_set_test(gsGlobal, GS_ATEST_ON);
                        gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
                        gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
                    }

                frame:
                    drawFrame(x, y, x + 72, (y + 55), color);

                    Len = printXY(tmp, 0, 0, 0, FALSE, 0);
                    if (Len > 72 && top + i == jpg_browser_sel) {
                        if (t % 0x10 == 0) {
                            strcpy(tmp1, tmp + print_name);
                            tmp1[10] = '\0';
                            if (++print_name > (Len / FONT_WIDTH - 10))
                                print_name = 0;
                        }
                        Len = printXY(tmp1, 0, 0, 0, FALSE, 0);
                        printXY(tmp1, x + 72 / 2 - Len / 2, (y + 58), color, TRUE, 0);
                    } else if (Len > 72) {
                        tmp[7] = '.';
                        tmp[8] = '.';
                        tmp[9] = '.';
                        tmp[10] = '\0';
                        printXY(tmp, x - 4, (y + 58), color, TRUE, 0);
                    } else
                        printXY(tmp, x + 72 / 2 - Len / 2, (y + 58), color, TRUE, 0);

                    if (TV_mode == TV_mode_NTSC)
                        y += 71 + 15;
                    else if (TV_mode == TV_mode_PAL)
                        y += 71 + 11;

                } /* end else THUMBNAIL */

            }  //ends for, so all browser rows were fixed above
            thumb_load = FALSE;
            if (jpg_browser_nfiles > rows) {  //if more files than available rows, use scrollbar
                drawFrame(SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 8, Frame_start_y,
                          SCREEN_WIDTH - SCREEN_MARGIN, Frame_end_y, setting->color[1]);
                y0 = (Menu_end_y - Menu_start_y + 8) * ((double)top / jpg_browser_nfiles);
                y1 = (Menu_end_y - Menu_start_y + 8) * ((double)(top + rows) / jpg_browser_nfiles);
                drawOpSprite(setting->color[1],
                             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 6, (y0 + Menu_start_y - 4),
                             SCREEN_WIDTH - SCREEN_MARGIN - LINE_THICKNESS * 2, (y1 + Menu_start_y - 4));
            }  //ends clause for scrollbar
            msg0[0] = '\0';
            if (jpg_browser_pushed)
                sprintf(msg0, "%s %s: %s", LNG(Jpg_Viewer), LNG(Path), path);

            //Tooltip section
            msg1[0] = '\0';
            if (swapKeys)
                sprintf(msg1, "ÿ1:%s", LNG(View));
            else
                sprintf(msg1, "ÿ0:%s", LNG(View));
            if (jpg_browser_mode == LIST)
                sprintf(tmp, " ÿ3:%s ÿ2:%s", LNG(Up), LNG(Thumb));
            else
                sprintf(tmp, " ÿ3:%s ÿ2:%s", LNG(Up), LNG(List));
            strcat(msg1, tmp);
            sprintf(tmp, " Sel:%s Start:%s L1/R1:%dsec L2:",
                    LNG(Exit), LNG(SlideShow), SlideShowTime);
            strcat(msg1, tmp);
            if (SlideShowTrans == OFF)
                strcat(msg1, LNG(Off));
            else if (SlideShowTrans == ZOOM)
                strcat(msg1, LNG(Zoom));
            else if (SlideShowTrans == FADE)
                strcat(msg1, LNG(Fade));
            else if (SlideShowTrans == ZOOM_FADE)
                strcat(msg1, LNG(ZoomFade));
            sprintf(tmp, " R2:%s", LNG(PathPad));
            strcat(msg1, tmp);
            setScrTmp(msg0, msg1);
        }  //ends if(event||post_event)
        drawScr();
        post_event = event;
        event = 0;
    }  //ends while
} /* end JpgViewer */
//--------------------------------------------------------------
//End of file: jpgviewer.c
//--------------------------------------------------------------
