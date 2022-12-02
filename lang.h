//---------------------------------------------------------------------------
// File name:    lang.h                  Revision Date: 2007.05.10
//---------------------------------------------------------------------------
// This file is used both for compiling the wLaunchELF program,
// with default language definitions, and when loading alternate
// language definitions at runtime. For the latter case it is the
// intention that an edited version of this file be used, where
// each string constant has been replaced by one suitable for the
// language to be used. Only the quoted strings should be edited
// for such use, as changing other parts of the lines could cause
// malfunction. The index number controls where in the program a
// string gets used, and the 'name' part is an identifying symbol
// that should remain untranslated even for translated versions,
// so that we can use them as a common reference independent both
// of the language used, and of the index number. Those names do
// not have any effect on loading alternate language definitions.
//--------------------------------------------------------------
// clang-format off
lang(0, Net_Config, "Net Config")
lang(1, Init_Delay, "Init Delay")
lang(2, TIMEOUT, "TIMEOUT")
lang(3, Halted, "Halted")
lang(4, LEFT, "LEFT")
lang(5, RIGHT, "RIGHT")
lang(6, PUSH_ANY_BUTTON_or_DPAD, "PUSH ANY BUTTON or D-PAD")
lang(7, Loading_HDL_Info_Module, "Loading HDL Info Module...")
lang(8, Loading_HDD_Modules, "Loading HDD Modules...")
lang(9, Loading_NetFS_and_FTP_Server_Modules, "Loading NetFS and FTP Server Modules...")
lang(10, Valid, "Valid")
lang(11, Bogus, "Bogus")
lang(12, CNF_Path, "CNF Path")
lang(13, is_Not_Found, "is Not Found")
//--------------------------------------------------------------
lang(14, MISC, "MISC")
lang(15, PS2Disc, "PS2Disc")
lang(16, FileBrowser, "FileBrowser")
lang(17, PS2Browser, "PS2Browser")
lang(18, PS2Net, "PS2Net")
lang(19, PS2PowerOff, "PS2PowerOff")
lang(20, HddManager, "HddManager")
lang(21, TextEditor, "TextEditor")
lang(22, JpgViewer, "JpgViewer")
lang(23, Configure, "Configure")
lang(24, Load_CNFprev, "Load CNF--")
lang(25, Load_CNFnext, "Load CNF++")
lang(26, Set_CNF_Path, "Set CNF_Path")
lang(27, Load_CNF, "Load CNF")
lang(28, ShowFont, "ShowFont")
//--------------------------------------------------------------
lang(29, Reading_SYSTEMCNF, "Reading SYSTEM.CNF...")
lang(30, Failed, "Failed")
lang(31, Powering_Off_Console, "Powering Off Console...")
lang(32, No_Disc, "No Disc")
lang(33, Detecting_Disc, "Detecting Disc")
lang(34, Stop_Disc, "Stop Disc")
lang(35, auto, "auto")
lang(36, Circle, "Circle")
lang(37, Cross, "Cross")
lang(38, Square, "Square")
lang(39, Triangle, "Triangle")
lang(40, Failed_To_Save, "Failed To Save")
lang(41, Saved_Failed, "Saved Failed")
lang(42, Saved_Config, "Saved Config")
lang(43, Saved, "Saved")
lang(44, Failed_writing, "Failed writing")
lang(45, Failed_To_Load, "Failed To Load")
lang(46, Loaded_Config, "Loaded Config")
//--------------------------------------------------------------
lang(47, SKIN_SETTINGS, "SKIN SETTINGS")
lang(48, Skin_Path, "Skin Path")
lang(49, Apply_New_Skin, "Apply New Skin")
lang(50, Brightness, "Brightness")
lang(51, Edit, "Edit")
lang(52, Clear, "Clear")
lang(53, Add, "Add")
lang(54, Subtract, "Subtract")
lang(55, OK, "OK")
lang(56, CANCEL, "CANCEL")
lang(57, Cancel, "Cancel")
lang(58, Browse, "Browse")
lang(59, Change, "Change")
lang(60, Return, "Return")
lang(61, PasteRename, "Paste+Rename")
lang(62, Back, "Back")
lang(63, Use, "Use")
lang(64, Set, "Set")
lang(65, Page_leftright, "Page left/right")
lang(66, Edit_Title, "Edit Title")
lang(67, Left, "Left")
lang(68, Right, "Right")
lang(69, Up, "Up")
lang(70, Enter, "Enter")
lang(71, Choose, "Choose")
lang(72, RevMark, "RevMark")
lang(73, TitleOFF, "TitleOFF")
lang(74, TitleON, "TitleON")
lang(75, Menu, "Menu")
lang(76, PathPad, "PathPad")
lang(77, Exit, "Exit")
lang(78, View, "View")
lang(79, Thumb, "Thumb")
lang(80, List, "List")
lang(81, SlideShow, "SlideShow")
lang(82, Sel, "Sel")
lang(83, BackSpace, "BackSpace")
lang(84, Mark, "Mark")
lang(85, CrLf, "Cr/Lf")
lang(86, Cr, "Cr")
lang(87, Lf, "Lf")
lang(88, Ins, "Ins")
lang(89, On, "On")
lang(90, Off, "Off")
lang(91, Open_KeyBoard, "Open KeyBoard")
lang(92, Close_KB, "Close KB")
lang(93, free, "free")
lang(94, Color, "Color")
lang(95, Backgr, "Backgr")
lang(96, Frames, "Frames")
lang(97, Select, "Select")
lang(98, Normal, "Normal")
lang(99, Graph, "Graph")
lang(100, TV_mode, "TV mode")
lang(101, Screen_X_offset, "Screen X offset")
lang(102, Screen_Y_offset, "Screen Y offset")
lang(103, Interlace, "Interlace")
lang(104, ON, "ON")
lang(105, OFF, "OFF")
lang(106, Skin_Settings, "Skin Settings")
lang(107, Menu_Title, "Menu Title")
lang(108, NULL, "NULL")
lang(109, NONE, "NONE")
lang(110, DEFAULT, "DEFAULT")
lang(111, Menu_Frame, "Menu Frame")
lang(112, Popups_Opaque, "Popups Opaque")
lang(113, Use_Default_Screen_Settings, "Use Default Screen Settings")
//--------------------------------------------------------------
lang(114, STARTUP_SETTINGS, "STARTUP SETTINGS")
lang(115, Reset_IOP, "Reset IOP")
lang(116, Number_of_CNFs, "Number of CNF's")
lang(117, Pad_mapping, "Pad mapping")
lang(118, USBD_IRX, "USBD IRX")
lang(119, Initial_Delay, "Initial Delay")
lang(120, Default_Timeout, "'Default' Timeout")
lang(121, USB_Keyboard_Used, "USB Keyboard Used")
lang(122, USB_Keyboard_IRX, "USB Keyboard IRX")
lang(123, USB_Keyboard_Map, "USB Keyboard Map")
lang(124, CNF_Path_override, "CNF Path override")
lang(125, USB_Mass_IRX, "USB Mass IRX")
lang(126, Language_File, "Language File")
//--------------------------------------------------------------
lang(127, NETWORK_SETTINGS, "NETWORK SETTINGS")
lang(128, IP_Address, "IP Address")
lang(129, Netmask, "Netmask")
lang(130, Gateway, "Gateway")
lang(131, Save_to, "Save to")
//--------------------------------------------------------------
lang(132, Right_DPad_to_Edit, "Right D-Pad to Edit")
lang(133, Button_Settings, "Button Settings")
lang(134, Show_launch_titles, "Show launch titles")
lang(135, Disc_control, "Disc control")
lang(136, Hide_full_ELF_paths, "Hide full ELF paths")
lang(137, Screen_Settings, "Screen Settings")
lang(138, Startup_Settings, "Startup Settings")
lang(139, Network_Settings, "Network Settings")
lang(140, Copy, "Copy")
lang(141, Cut, "Cut")
lang(142, Paste, "Paste")
lang(143, Delete, "Delete")
lang(144, New_Dir, "New Dir")
lang(145, Get_Size, "Get Size")
lang(146, mcPaste, "mcPaste")
lang(147, psuPaste, "psuPaste")
lang(148, Name_conflict_for_this_folder, "Name conflict for this folder")
lang(149, With_gamesave_title, "With gamesave title")
lang(150, Undefined_Not_a_gamesave, "Undefined (Not a gamesave)")
lang(151, Do_you_wish_to_overwrite_it, "Do you wish to overwrite it")
lang(152, Pasting_file, "Pasting file")
lang(153, Remain_Size, "RemainSize")
lang(154, bytes, "bytes")
lang(155, Kbytes, "Kbytes")
lang(156, Current_Speed, "Current Speed")
lang(157, Unknown, "Unknown")
lang(158, Remain_Time, "RemainTime")
lang(159, Written_Total, "Written Total")
lang(160, Average_Speed, "Average Speed")
lang(161, Continue_transfer, "Continue transfer ?")
lang(162, This_file_isnt_an_ELF, "This file isn't an ELF")
lang(163, Copied_to_the_Clipboard, "Copied to the Clipboard")
lang(164, Mark_Files_Delete, "Mark Files Delete ?")
lang(165, deleting, "deleting")
lang(166, Delete_Failed, "Delete Failed")
lang(167, Rename_Failed, "Rename Failed")
lang(168, directory_already_exists, "directory already exists")
lang(169, NewDir_Failed, "NewDir Failed")
lang(170, Created_folder, "Created folder")
lang(171, Path, "Path")
lang(172, Checking_Size, "Checking Size...")
lang(173, size_result, "size result")
lang(174, Size_test_Failed, "Size test Failed")
lang(175, mctype, "mctype")
lang(176, Pasting, "Pasting...")
lang(177, Paste_Failed, "Paste Failed")
lang(178, pasting, "pasting")
lang(179, Reading_HDD_Information, "Reading HDD Information...")
lang(180, Found, "Found")
lang(181, MB, "MB")
lang(182, GB, "GB")
lang(183, HDD_Information_Read, "HDD Information Read")
lang(184, Create, "Create")
lang(185, Remove, "Remove")
lang(186, Rename, "Rename")
lang(187, Expand, "Expand")
lang(188, Format, "Format")
lang(189, Creating_New_Partition, "Creating New Partition...")
lang(190, New_Partition_Created, "New Partition Created")
lang(191, Failed_Creating_New_Partition, "Failed Creating New Partition")
lang(192, Removing_Current_Partition, "Removing Current Partition...")
lang(193, Partition_Removed, "Partition Removed")
lang(194, Failed_Removing_Current_Partition, "Failed Removing Current Partition")
lang(195, Renaming_Partition, "Renaming Partition...")
lang(196, Partition_Renamed, "Partition Renamed")
lang(197, Failed_Renaming_Partition, "Failed Renaming Partition")
lang(198, Renaming_Game, "Renaming Game...")
lang(199, Game_Renamed, "Game Renamed")
lang(200, Failed_Renaming_Game, "Failed Renaming Game")
lang(201, Expanding_Current_Partition, "Expanding Current Partition...")
lang(202, Partition_Expanded, "Partition Expanded")
lang(203, Failed_Expanding_Current_Partition, "Failed Expanding Current Partition")
lang(204, Formating_HDD, "Formating HDD...")
lang(205, HDD_Formated, "HDD Formated")
lang(206, HDD_Format_Failed, "HDD Format Failed")
lang(207, Create_New_Partition, "Create New Partition ?")
lang(208, Remove_Current_Partition, "Remove Current Partition ?")
lang(209, Enter_New_Partition_Name, "Enter New Partition Name.")
lang(210, Rename_Current_Game, "Rename Current Game ?")
lang(211, Rename_Current_Partition, "Rename Current Partition ?")
lang(212, Select_New_Partition_Size_In_MB, "Select New Partition Size In MB.")
lang(213, Expand_Current_Partition, "Expand Current Partition ?")
lang(214, Format_HDD, "Format entire HDD (destroys all partitions) ?")
lang(215, HDD_STATUS, "HDD STATUS")
lang(216, CONNECTED, "CONNECTED")
lang(217, FORMATED, "FORMATED")
lang(218, YES, "YES")
lang(219, NO, "NO")
lang(220, HDD_SIZE, "HDD SIZE")
lang(221, HDD_USED, "HDD USED")
lang(222, HDD_FREE, "HDD FREE")
lang(223, FREE, "FREE")
lang(224, Raw_SIZE, "Raw SIZE")
lang(225, Reserved_for_system, "Reserved for system")
lang(226, Inaccessible, "Inaccessible")
lang(227, HDL_SIZE, "HDL SIZE")
lang(228, Info_not_loaded, "Info not loaded")
lang(229, GAME_INFORMATION, "GAME INFORMATION")
lang(230, STARTUP, "STARTUP")
lang(231, SIZE, "SIZE")
lang(232, TYPE_DVD_GAME, "TYPE: DVD GAME")
lang(233, TYPE_CD_GAME, "TYPE: CDGAME")
lang(234, PFS_SIZE, "PFS SIZE")
lang(235, PFS_USED, "PFS USED")
lang(236, PFS_FREE, "PFS FREE")
lang(237, MENU, "MENU")
lang(238, Load_HDL_Game_Info, "Load HDL Game Info")
lang(239, PS2_HDD_MANAGER, "PS2 HDD MANAGER")
lang(240, New, "New")
lang(241, Open, "Open")
lang(242, Close, "Close")
lang(243, Save, "Save")
lang(244, Save_As, "Save As")
lang(245, Windows, "Windows")
lang(246, Free_Window, "Free Window")
lang(247, Window_Not_Yet_Saved, "Window Not Yet Saved")
lang(248, File_Created, "File Created")
lang(249, Failed_Creating_File, "Failed Creating File")
lang(250, File_Opened, "File Opened")
lang(251, Failed_Opening_File, "Failed Opening File")
lang(252, File_Not_Yet_Saved, "File Not Yet Saved")
lang(253, File_Not_Yet_Saved_Closed, "File Not Yet Saved Closed")
lang(254, File_Closed, "File %s Closed")
lang(255, File_Saved, "File Saved")
lang(256, Failed_Saving_File, "Failed Saving File")
lang(257, Enter_File_Name, "Enter File Name.")
lang(258, Exiting_Editor, "Exiting Editor...")
lang(259, Exit_Without_Saving, "Exit Without Saving ?")
lang(260, Select_A_File_For_Editing, "Select A File For Editing")
lang(261, Close_Without_Saving, "Close Without Saving ?")
lang(262, MARK, "MARK")
lang(263, COPY, "COPY")
lang(264, CUT, "CUT")
lang(265, PASTE, "PASTE")
lang(266, LINE_UP, "LINE UP")
lang(267, LINE_DOWN, "LINE DOWN")
lang(268, PAGE_UP, "PAGE UP")
lang(269, PAGE_DOWN, "PAGE DOWN")
lang(270, HOME, "HOME")
lang(271, END, "END")
lang(272, INSERT, "INSERT")
lang(273, RET_CRLF, "RET CR/LF")
lang(274, RET_CR, "RET CR")
lang(275, RET_LF, "RET LF")
lang(276, TAB, "TAB")
lang(277, SPACE, "SPACE")
lang(278, RETURN, "RETURN")
lang(279, PS2_TEXT_EDITOR, "PS2 TEXT EDITOR")
lang(280, Start_StartStop_Slideshow, "Start: Start/Stop Slideshow")
lang(281, L1R1_Slideshow_Timer, "L1/R1: Slideshow Timer")
lang(282, L2R2_Slideshow_Transition, "L2/R2: Slideshow Transition")
lang(283, LeftRight_Pad_PrevNext_Picture, "Left/Right Pad: Prev/Next Picture")
lang(284, UpDown_Pad_Rotate_Picture, "Up/Down Pad: Rotate Picture")
lang(285, Left_Joystick_Panorama, "Left Joystick: Panorama")
lang(286, Right_Joystick_Vertical_Zoom, "Right Joystick Vertical: Zoom")
lang(287, FullScreen_Mode, "FullScreen Mode")
lang(288, Exit_To_Jpg_Browser, "Exit To Jpg Browser")
lang(289, Jpg_Viewer, "Jpg Viewer")
lang(290, Picture, "Picture")
lang(291, Size, "Size")
lang(292, Command_List, "Command List")
lang(293, Timer, "Timer")
lang(294, Transition, "Transition")
lang(295, Zoom, "Zoom")
lang(296, Fade, "Fade")
lang(297, ZoomFade, "Zoom+Fade")
//--------------------------------------------------------------
// Startup Setting Font file Added
lang(298, Font_File, "Font File")
//--------------------------------------------------------------
// New MISC entry for debug info
// Note that substrings in that info screen should NEVER have
// any language definitions made for them, as they can and will
// differ between versions, depending on current debug needs.
// So only the name of the MISC entry itself should be defined.
lang(299, Debug_Info, "Debug Info")
//--------------------------------------------------------------
// New FileBrowser display modes and sort modes added
lang(300, Mode, "Mode")
lang(301, Show_Content_as, "Show Content as")
lang(302, Filename, "Filename")
lang(303, Game_Title, "Game Title")
lang(304, _plus_Details, " + Details")
lang(305, Sort_Content_by, "Sort Content by")
lang(306, No_Sort, "No Sort")
lang(307, Timestamp, "Timestamp")
lang(308, Back_to_Browser, "Back to Browser")
//--------------------------------------------------------------
// New Icon setup command added
lang(309, New_Icon, "New Icon")
lang(310, file_alredy_exists, "file already exists")
lang(311, Icon_Title, "Icon Title")
lang(312, IconText, "IconText")
//--------------------------------------------------------------
lang(313, KB_RETURN, "RETURN")
//--------------------------------------------------------------
// New GUI Skin option added
lang(314, GUI, "GUI")
lang(315, Apply_GUI_Skin, "Apply GUI Skin")
lang(316, Skin_Preview, "Skin Preview")
lang(317, Halt, "Halt")  // Alternative Halted text, added for main screen skinning purposes, only appears when main screen skin is active
lang(318, Delay, "Delay")            // Same reason as above
lang(319, Show_Menu, "Show Menu")
//---------------------------------------------------------------------------
// New skin file commands added
lang(320, Load_Skin_CNF, "Load Skin CNF")
lang(321, Save_Skin_CNF, "Save Skin CNF")
//---------------------------------------------------------------------------
// New FileBrowser command for virtual memory cards
lang(322, Mount, "Mount")
//---------------------------------------------------------------------------
// New MISC entry for "About uLE" screen display
lang(323, About_uLE, "About uLE")
//---------------------------------------------------------------------------
// New MISC entry for "OSDSYS(MC)" launch (for FMCB on incompatible consoles)
lang(324, OSDSYS, "OSDSYS")
// for hdl_game_unload
lang(325, Unload_HDL_Game_Info, "Unload HDL Game Info")
//---------------------------------------------------------------------------
// New status message for HDD information read, when there are too many partitions.
lang(326, HDD_Information_Read_Overflow, "HDD Information Read (truncated)")

    // clang-format on
    //---------------------------------------------------------------------------
    // End of file:  lang.h
    //---------------------------------------------------------------------------
