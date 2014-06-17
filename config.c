#include "launchelf.h"

enum
{
	DEF_TIMEOUT = 10,
	DEF_FILENAME = TRUE,
	DEF_COLOR1 = ITO_RGBA(30,30,50,0),
	DEF_COLOR2 = ITO_RGBA(85,85,95,0),
	DEF_COLOR3 = ITO_RGBA(160,160,160,0),
	DEF_COLOR4 = ITO_RGBA(255,255,255,0),
	DEF_SCREEN_X = 128,
	DEF_SCREEN_Y = 30,
	DEF_DISCCONTROL = TRUE,
	DEF_INTERLACE = FALSE,
	DEF_RESETIOP = FALSE,
	DEF_NUMCNF = 1,
	DEF_SWAPKEYS = FALSE,
	
	DEFAULT=0,
	TIMEOUT=12,
	DISCCONTROL,
	FILENAME,
	SCREEN,
	SETTINGS,
	OK,
	CANCEL
};

SETTING *setting = NULL;
SETTING *tmpsetting;

////////////////////////////////////////////////////////////////////////
// メモリーカードの状態をチェックする。
// 戻り値は有効なメモリーカードスロットの番号。
// メモリーカードが挿さっていない場合は-11を返す。
int CheckMC(void)
{
	int dummy, ret;
	
	mcGetInfo(0, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if( -1 == ret || 0 == ret) return 0;

	mcGetInfo(1, 0, &dummy, &dummy, &dummy);
	mcSync(0, NULL, &ret);

	if( -1 == ret || 0 == ret ) return 1;

	return -11;
}

////////////////////////////////////////////////////////////////////////
// LAUNCHELF.CNF に設定を保存する
void saveConfig(char *mainMsg, char *CNF)
{
	int i, ret, fd, size;
	const char LF[3]={0x0D, 0x0A, 0};
	char c[MAX_PATH], tmp[25][MAX_PATH];
	char* p;
	
	// 設定をメモリに格納
	for(i=0; i<12; i++)
		strcpy(tmp[i], setting->dirElf[i]);
	sprintf(tmp[12], "%d", setting->timeout);
	sprintf(tmp[13], "%d", setting->filename);
	for(i=0; i<4; i++)
		sprintf(tmp[14+i], "%lu", setting->color[i]);
	sprintf(tmp[18], "%d", setting->screen_x);
	sprintf(tmp[19], "%d", setting->screen_y);
	sprintf(tmp[20], "%d", setting->discControl);
	sprintf(tmp[21], "%d", setting->interlace);
	sprintf(tmp[22], "%d", setting->resetIOP);
	sprintf(tmp[23], "%d", setting->numCNF);
	sprintf(tmp[24], "%d", setting->swapKeys);
	
	p = (char*)malloc(sizeof(SETTING));
	p[0]=0;
	size=0;
	for(i=0; i<25; i++){
		strcpy(c, tmp[i]);
		strcat(c, LF);
		strcpy(p+size, c);
		size += strlen(c);
	}
	// LaunchELFのディレクトリにCNFがあったらLaunchELFのディレクトリにセーブ
	strcpy(c, LaunchElfDir);
	strcat(c, CNF);
	if((fd=fioOpen(c, O_RDONLY)) >= 0)
		fioClose(fd);
	else{
		if(!strncmp(LaunchElfDir, "mc", 2))
			sprintf(c, "mc%d:/SYS-CONF", LaunchElfDir[2]-'0');
		else
			sprintf(c, "mc%d:/SYS-CONF", CheckMC());
		// SYS-CONFがあったらSYS-CONFにセーブ
		if((fd=fioDopen(c)) >= 0){
			fioDclose(fd);
			char strtmp[MAX_PATH] = "/";
			strcat(c, strcat(strtmp, CNF));
		// SYS-CONFがなかったらLaunchELFのディレクトリにセーブ
		}else{
			strcpy(c, LaunchElfDir);
			strcat(c, CNF);
		}
	}
	sprintf(mainMsg, "Failed To Save %s", CNF);
	// 書き込み
	if((fd=fioOpen(c,O_CREAT|O_WRONLY|O_TRUNC)) < 0){
		return;
	}
	ret = fioWrite(fd,p,size);
	if(ret==size) sprintf(mainMsg, "Save Config (%s)", c);
	fioClose(fd);
	free(p);
}

////////////////////////////////////////////////////////////////////////
// LAUNCHELF.CNF から設定を読み込む
void loadConfig(char *mainMsg, char *CNF)
{
	int i, j, k, fd, len, mcport;
	size_t size;
	char path[MAX_PATH], tmp[25][MAX_PATH], *p;
	
	if(setting!=NULL)
		free(setting);
	setting = (SETTING*)malloc(sizeof(SETTING));
	// LaunchELFが実行されたパスから設定ファイルを開く
	strcpy(path, LaunchElfDir);
	strcat(path, CNF);
	if(!strncmp(path, "cdrom", 5)) strcat(path, ";1");
	fd = fioOpen(path, O_RDONLY);
	// 開けなかったら、SYS-CONFの設定ファイルを開く
	if(fd<0) {
		if(!strncmp(LaunchElfDir, "mc", 2))
			mcport = LaunchElfDir[2]-'0';
		else
			mcport = CheckMC();
		if(mcport==1 || mcport==0){
			char strtmp[MAX_PATH];
			sprintf(strtmp, "mc%d:/SYS-CONF/", mcport);
			strcpy(path, strtmp);
			strcat(path, CNF);
			fd = fioOpen(path, O_RDONLY);
			if(fd>=0)
				strcpy(LaunchElfDir, strtmp);
		}
	}
	// どのファイルも開けなかった場合、設定を初期化する
	if(fd<0) {
		for(i=0; i<12; i++)
			setting->dirElf[i][0] = 0;
		setting->timeout = DEF_TIMEOUT;
		setting->filename = DEF_FILENAME;
		setting->color[0] = DEF_COLOR1;
		setting->color[1] = DEF_COLOR2;
		setting->color[2] = DEF_COLOR3;
		setting->color[3] = DEF_COLOR4;
		setting->screen_x = DEF_SCREEN_X;
		setting->screen_y = DEF_SCREEN_Y;
		setting->discControl = DEF_DISCCONTROL;
		setting->interlace = DEF_INTERLACE;
		setting->resetIOP = DEF_RESETIOP;
		setting->numCNF = DEF_NUMCNF;
		setting->swapKeys = DEF_SWAPKEYS;
		sprintf(mainMsg, "Failed To Load %s", CNF);
	} else {
		// 設定ファイルをメモリに読み込み
		size = fioLseek(fd, 0, SEEK_END);
		printf("size=%d\n", size);
		fioLseek(fd, 0, SEEK_SET);
		p = (char*)malloc(size);
		fioRead(fd, p, size);
		fioClose(fd);
		
		// 計22行のテキストを読み込む
		// 12行目まではボタンセッティング
		// 13行目は TIMEOUT 値
		// 14行目は PRINT ONLY FILENAME の設定値
		// 15行目はカラー1
		// 16行目はカラー2
		// 17行目はカラー3
		// 18行目はカラー4
		// 19行目はスクリーンX
		// 20行目はスクリーンY
		// 21行目はディスクコントロール
		// 22行目はインターレース
		for(i=j=k=0; i<size; i++) {
			if(p[i]==0x0D && p[i+1]==0x0A) {
				if(i-k<MAX_PATH) {
					p[i]=0;
					strcpy(tmp[j++], &p[k]);
				} else
					break;
				if(j>=25)
					break;
				k=i+2;
			}
		}
		while(j<25)
			tmp[j++][0] = 0;
		// ボタンセッティング
		for(i=0; i<12; i++) {
			// v3.01以前のバージョンとの上位互換
			if(tmp[i][0] == '/')
				sprintf(setting->dirElf[i], "mc:%s", tmp[i]);
			else
				strcpy(setting->dirElf[i], tmp[i]);
		}
		// TIMEOUT値の設定
		if(tmp[12][0]) {
			setting->timeout = 0;
			len = strlen(tmp[12]);
			i = 1;
			while(len-- != 0) {
				setting->timeout += (tmp[12][len]-'0') * i;
				i *= 10;
			}
		} else
			setting->timeout = DEF_TIMEOUT;
		// PRINT ONLY FILENAME の設定
		if(tmp[13][0])
			setting->filename = tmp[13][0]-'0';
		else
			setting->filename = DEF_FILENAME;
		// カラー1から4の設定
		if(tmp[14][0]) {
			for(i=0; i<4; i++) {
				setting->color[i] = 0;
				len = strlen(tmp[14+i]);
				j = 1;
				while(len-- != 0) {
					setting->color[i] += (tmp[14+i][len]-'0') * j;
					j *= 10;
				}
			}
		} else {
			setting->color[0] = DEF_COLOR1;
			setting->color[1] = DEF_COLOR2;
			setting->color[2] = DEF_COLOR3;
			setting->color[3] = DEF_COLOR4;
		}
		// スクリーンXの設定
		if(tmp[18][0]) {
			setting->screen_x = 0;
			j = strlen(tmp[18]);
			for(i=1; j; i*=10)
				setting->screen_x += (tmp[18][--j]-'0')*i;
		} else
			setting->screen_x = DEF_SCREEN_X;
		// スクリーンYの設定
		if(tmp[19][0]) {
			setting->screen_y = 0;
			j = strlen(tmp[19]);
			for(i=1; j; i*=10)
				setting->screen_y += (tmp[19][--j]-'0')*i;
		} else
			setting->screen_y = DEF_SCREEN_Y;
		// ディスクコントロールの設定
		if(tmp[20][0])
			setting->discControl = tmp[20][0]-'0';
		else
			setting->discControl = DEF_DISCCONTROL;
		// インターレースの設定
		if(tmp[21][0])
			setting->interlace = tmp[21][0]-'0';
		else
			setting->interlace = DEF_INTERLACE;
		// Reset IOP
		if(tmp[22][0])
			setting->resetIOP = tmp[22][0]-'0';
		else
			setting->resetIOP = DEF_RESETIOP;
		// Num CNF's
		if(tmp[23][0]) {
			setting->numCNF = 0;
			j = strlen(tmp[23]);
			for(i=1; j; i*=10)
				setting->numCNF += (tmp[23][--j]-'0')*i;
		} else
			setting->numCNF = DEF_NUMCNF;
		// Swap keys
		if(tmp[24][0])
			setting->swapKeys = tmp[24][0]-'0';
		else
			setting->swapKeys = DEF_SWAPKEYS;
		free(p);
		sprintf(mainMsg, "Load Config (%s)", path);
	}
	return;
}

////////////////////////////////////////////////////////////////////////
// スクリーンセッティング画面
void setColor(void)
{
	int i;
	int s;
	int x, y;
	uint64 rgb[4][3];
	char c[MAX_PATH];
	
	for(i=0; i<4; i++) {
		rgb[i][0] = setting->color[i] & 0xFF;
		rgb[i][1] = setting->color[i] >> 8 & 0xFF;
		rgb[i][2] = setting->color[i] >> 16 & 0xFF;
	}
	
	s=0;
	while(1)
	{
		// 操作
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				if(s!=0)
					s--;
				else
					s=16;
			}
			else if(new_pad & PAD_DOWN)
			{
				if(s!=16)
					s++;
				else
					s=0;
			}
			else if(new_pad & PAD_LEFT)
			{
				if(s>=15) s=14;
				else if(s>=14) s=12;
				else if(s>=12) s=9;
				else if(s>=9) s=6;
				else if(s>=6) s=3;
				else if(s>=3) s=0;
			}
			else if(new_pad & PAD_RIGHT)
			{
				if(s>=14) s=15;
				else if(s>=12) s=14;
				else if(s>=9) s=12;
				else if(s>=6) s=9;
				else if(s>=3) s=6;
				else s=3;
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				if(s<12) {
					if(rgb[s/3][s%3] > 0) {
						rgb[s/3][s%3]--;
						setting->color[s/3] = 
							ITO_RGBA(rgb[s/3][0], rgb[s/3][1], rgb[s/3][2], 0);
						if(s/3 == 0) itoSetBgColor(setting->color[0]);
					}
				} else if(s==12) {
					if(setting->screen_x > 0) {
						setting->screen_x--;
						screen_env.screen.x = setting->screen_x;
						itoSetScreenPos(setting->screen_x, setting->screen_y);
					}
				} else if(s==13) {
					if(setting->screen_y > 0) {
						setting->screen_y--;
						screen_env.screen.y = setting->screen_y;
						itoSetScreenPos(setting->screen_x, setting->screen_y);
					}
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				if(s<12) {
					if(rgb[s/3][s%3] < 255) {
						rgb[s/3][s%3]++;
						setting->color[s/3] = 
							ITO_RGBA(rgb[s/3][0], rgb[s/3][1], rgb[s/3][2], 0);
						if(s/3 == 0) itoSetBgColor(setting->color[0]);
					}
				} else if(s==12) {
					setting->screen_x++;
					screen_env.screen.x = setting->screen_x;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==13) {
					setting->screen_y++;
					screen_env.screen.y = setting->screen_y;
					itoSetScreenPos(setting->screen_x, setting->screen_y);
				} else if(s==14) {
					setting->interlace = !setting->interlace;
					screen_env.interlace = setting->interlace;
					itoGsReset();
					itoGsEnvSubmit(&screen_env);
				} else if(s==15) {
					return;
				} else if(s==16) {
					setting->color[0] = DEF_COLOR1;
					setting->color[1] = DEF_COLOR2;
					setting->color[2] = DEF_COLOR3;
					setting->color[3] = DEF_COLOR4;
					setting->screen_x = DEF_SCREEN_X;
					setting->screen_y = DEF_SCREEN_Y;
					setting->interlace = DEF_INTERLACE;
					screen_env.screen.x = setting->screen_x;
					screen_env.screen.y = setting->screen_y;
					screen_env.interlace = setting->interlace;
					itoGsReset();
					itoGsEnvSubmit(&screen_env);
					itoSetBgColor(setting->color[0]);
					//itoSetScreenPos(setting->screen_x, setting->screen_y);
					
					for(i=0; i<4; i++) {
						rgb[i][0] = setting->color[i] & 0xFF;
						rgb[i][1] = setting->color[i] >> 8 & 0xFF;
						rgb[i][2] = setting->color[i] >> 16 & 0xFF;
					}
				}
			}
		}
		
		// 画面描画開始
		clrScr(setting->color[0]);
		
		// 枠の中
		x = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
		y = SCREEN_MARGIN + FONT_HEIGHT*2 + LINE_THICKNESS + 12;
		for(i=0; i<4; i++)
		{
			sprintf(c, "  COLOR%d R: %lu", i+1, rgb[i][0]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "  COLOR%d G: %lu", i+1, rgb[i][1]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			sprintf(c, "  COLOR%d B: %lu", i+1, rgb[i][2]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
			y += FONT_HEIGHT / 2;
		}
		
		sprintf(c, "  SCREEN X: %d", setting->screen_x);
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		sprintf(c, "  SCREEN Y: %d", setting->screen_y);
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		y += FONT_HEIGHT / 2;
		
		if(setting->interlace)
			sprintf(c, "  INTERLACE: ON");
		else
			sprintf(c, "  INTERLACE: OFF");
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		y += FONT_HEIGHT / 2;
		
		printXY("  RETURN", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		printXY("  INITIAL SCREEN SETTINGS", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		y = s * FONT_HEIGHT + SCREEN_MARGIN+FONT_HEIGHT*2+12+LINE_THICKNESS;
		if(s>=3) y+=FONT_HEIGHT/2;
		if(s>=6) y+=FONT_HEIGHT/2;
		if(s>=9) y+=FONT_HEIGHT/2;
		if(s>=12) y+=FONT_HEIGHT/2;
		if(s>=14) y+=FONT_HEIGHT/2;
		if(s>=15) y+=FONT_HEIGHT/2;
		drawChar(127, x, y/2, setting->color[3]);
		
		x = SCREEN_MARGIN;
		y = SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT;
		if (s <= 13) {
			if (swapKeys)
				strcpy(c, "×:Add ○:Subtract");
			else
				strcpy(c, "○:Add ×:Subtract");
		} else if(s==14) {
			if (swapKeys)
				strcpy(c, "×:Change");
			else
				strcpy(c, "○:Change");
		} else {
			if (swapKeys)
				strcpy(c, "×:OK");
			else
				strcpy(c, "○:OK");
		}
		
		setScrTmp("", c);
		drawScr();
	}
}

////////////////////////////////////////////////////////////////////////
// Other settings by EP
void setSettings(void)
{
	int s;
	int x, y;
	char c[MAX_PATH];
	
	s=1;
	while(1)
	{
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				if(s!=1) s--;
				else s=4;
			}
			else if(new_pad & PAD_DOWN)
			{
				if(s!=4) s++;
				else s=1;
			}
			else if(new_pad & PAD_LEFT)
			{
				if(s!=4) s=4;
				else s=1;
			}
			else if(new_pad & PAD_RIGHT)
			{
				if(s!=4) s=4;
				else s=1;
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				if(s==2)
				{
					if(setting->numCNF > 1)
						setting->numCNF--;
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				if(s==1)
					setting->resetIOP = !setting->resetIOP;
				else if(s==2)
					setting->numCNF++;
				else if(s==3)
					setting->swapKeys = !setting->swapKeys;
				else
					return;
			}
		}
		
		clrScr(setting->color[0]);
		x = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
		y = SCREEN_MARGIN + FONT_HEIGHT*2 + LINE_THICKNESS + 12;
		
		printXY("STARTUP SETTINGS", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		if(setting->resetIOP)
			sprintf(c, "  RESET IOP: ON");
		else
			sprintf(c, "  RESET IOP: OFF");
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		sprintf(c, "  NUMBER OF CNF'S: %d", setting->numCNF);
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		if(setting->swapKeys)
			sprintf(c, "  KEY MAPPING: ×:OK ○:CANCEL");
		else
			sprintf(c, "  KEY MAPPING: ○:OK ×:CANCEL");
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		y += FONT_HEIGHT / 2;
		printXY("  RETURN", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		y = s * FONT_HEIGHT + SCREEN_MARGIN+FONT_HEIGHT*2+12+LINE_THICKNESS;
		
		if(s>=4) y+=FONT_HEIGHT/2;
		drawChar(127, x, y/2, setting->color[3]);
		
		if (s == 1) {
			if (swapKeys)
				strcpy(c, "×:Change");
			else
				strcpy(c, "○:Change");
		} else if (s == 2) {
			if (swapKeys)
				strcpy(c, "×:Add ○:Subtract");
			else
				strcpy(c, "○:Add ×:Subtract");
		} else if( s== 3) {
			if (swapKeys)
				sprintf(c, "×:Change");
			else
				sprintf(c, "○:Change");
		} else {
			if (swapKeys)
				strcpy(c, "×:OK");
			else
				strcpy(c, "○:OK");
		}
		
		setScrTmp("", c);
		drawScr();
	}
}

////////////////////////////////////////////////////////////////////////
// Config画面
void config(char *mainMsg, char *CNF)
{
	char c[MAX_PATH];
	int i;
	int s;
	int x, y;
	
	tmpsetting = setting;
	setting = (SETTING*)malloc(sizeof(SETTING));
	*setting = *tmpsetting;
	
	s=0;
	while(1)
	{
		// 操作
		waitPadReady(0, 0);
		if(readpad())
		{
			if(new_pad & PAD_UP)
			{
				if(s!=0)
					s--;
				else
					s=CANCEL;
			}
			else if(new_pad & PAD_DOWN)
			{
				if(s!=CANCEL)
					s++;
				else
					s=0;
			}
			else if(new_pad & PAD_LEFT)
			{
				if(s>=OK)
					s=TIMEOUT;
				else
					s=DEFAULT;
			}
			else if(new_pad & PAD_RIGHT)
			{
				if(s<TIMEOUT)
					s=TIMEOUT;
				else if(s<OK)
					s=OK;
			}
			else if((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE) )
			{
				if(s<TIMEOUT)
					setting->dirElf[s][0]=0;
				else if(s==TIMEOUT)
				{
					if(setting->timeout > 0)
						setting->timeout--;
				}
			}
			else if((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE))
			{
				if(s<TIMEOUT)
				{
					getFilePath(setting->dirElf[s], TRUE);
					if(!strncmp(setting->dirElf[s], "mc", 2)){
						sprintf(c, "mc%s", &setting->dirElf[s][3]);
						strcpy(setting->dirElf[s], c);
					}
				}
				else if(s==TIMEOUT)
					setting->timeout++;
				else if(s==FILENAME)
					setting->filename = !setting->filename;
				else if(s==DISCCONTROL)
					setting->discControl = !setting->discControl;
				else if(s==SCREEN)
					setColor();
				else if(s==SETTINGS)
					setSettings();
				else if(s==OK)
				{
					free(tmpsetting);
					saveConfig(mainMsg, CNF);
					break;
				}
				else if(s==CANCEL)
				{
					free(setting);
					setting = tmpsetting;
					screen_env.screen.x = setting->screen_x;
					screen_env.screen.y = setting->screen_y;
					screen_env.interlace = setting->interlace;
					itoGsReset();
					itoGsEnvSubmit(&screen_env);
					itoSetBgColor(setting->color[0]);
					mainMsg[0] = 0;
					break;
				}
			}
		}
		
		// 画面描画開始
		clrScr(setting->color[0]);
		
		// 枠の中
		x = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
		y = SCREEN_MARGIN + FONT_HEIGHT*2 + LINE_THICKNESS + 12;
		printXY("BUTTON SETTING", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		for(i=0; i<12; i++)
		{
			switch(i)
			{
			case 0:
				strcpy(c,"  DEFAULT: ");
				break;
			case 1:
				strcpy(c,"  ○     : ");
				break;
			case 2:
				strcpy(c,"  ×     : ");
				break;
			case 3:
				strcpy(c,"  □     : ");
				break;
			case 4:
				strcpy(c,"  △     : ");
				break;
			case 5:
				strcpy(c,"  L1     : ");
				break;
			case 6:
				strcpy(c,"  R1     : ");
				break;
			case 7:
				strcpy(c,"  L2     : ");
				break;
			case 8:
				strcpy(c,"  R2     : ");
				break;
			case 9:
				strcpy(c,"  L3     : ");
				break;
			case 10:
				strcpy(c,"  R3     : ");
				break;
			case 11:
				strcpy(c,"  START  : ");
				break;
			}
			strcat(c, setting->dirElf[i]);
			printXY(c, x, y/2, setting->color[3], TRUE);
			y += FONT_HEIGHT;
		}
		
		y += FONT_HEIGHT / 2;
		
		printXY("MISC", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		sprintf(c, "  TIMEOUT: %d", setting->timeout);
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		if(setting->discControl)
			sprintf(c, "  DISC CONTROL: ON");
		else
			sprintf(c, "  DISC CONTROL: OFF");
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		if(setting->filename)
			sprintf(c, "  PRINT ONLY FILENAME: ON");
		else
			sprintf(c, "  PRINT ONLY FILENAME: OFF");
		printXY(c, x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		printXY("  SCREEN SETTING", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		printXY("  STARTUP SETTINGS", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		
		printXY("  OK", x, y/2, setting->color[3], TRUE);
		y += FONT_HEIGHT;
		printXY("  CANCEL", x, y/2, setting->color[3], TRUE);
		
		y = s * FONT_HEIGHT + SCREEN_MARGIN+FONT_HEIGHT*3+12+LINE_THICKNESS;
		if(s>=TIMEOUT)
			y += FONT_HEIGHT + FONT_HEIGHT / 2;
		drawChar(127, x, y/2, setting->color[3]);
		
		if (s < TIMEOUT) {
			if (swapKeys)
				sprintf(c, "×:Edit ○:Clear");
			else
				sprintf(c, "○:Edit ×:Clear");
		} else if(s==TIMEOUT) {
			if (swapKeys)
				sprintf(c, "×:Add ○:Subtract");
			else
				sprintf(c, "○:Add ×:Subtract");
		} else if(s==FILENAME) { 
			if (swapKeys)
				sprintf(c, "×:Change");
			else
				sprintf(c, "○:Change");
		} else if(s==DISCCONTROL) {
			if (swapKeys)
				sprintf(c, "×:Change");
			else
				sprintf(c, "○:Change");
		} else {
			if (swapKeys)
				sprintf(c, "×:OK");
			else
				sprintf(c, "○:OK");
		}

		setScrTmp("", c);
		drawScr();
	}
	
	return;
}
