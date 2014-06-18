//--------------------------------------------------------------
//File name:   filer.c
//--------------------------------------------------------------
#include "launchelf.h"

typedef struct{
	char name[256];
	char title[16*4+1];
	mcTable stats;
} FILEINFO;

typedef struct
{
	unsigned char unknown;
	unsigned char sec;      // date/time (second)
	unsigned char min;      // date/time (minute)
	unsigned char hour;     // date/time (hour)
	unsigned char day;      // date/time (day)
	unsigned char month;    // date/time (month)
	unsigned short year;    // date/time (year)
} PS2TIME __attribute__((aligned (2)));

#define MC_ATTR_norm_folder 0x8427  //Normal folder on PS2 MC
#define MC_ATTR_prot_folder 0x842F  //Protected folder on PS2 MC
#define MC_ATTR_PS1_folder  0x9027  //PS1 save folder on PS2 MC
#define MC_ATTR_norm_file   0x8497  //file (PS2/PS1) on PS2 MC
#define MC_ATTR_PS1_file    0x9417  //PS1 save file on PS1 MC

enum
{
	COPY,
	CUT,
	PASTE,
	MCPASTE,
	DELETE,
	RENAME,
	NEWDIR,
	GETSIZE,
	NUM_MENU
};

#define PM_NORMAL     0 //PasteMode value for normal copies
#define PM_MC_BACKUP  1 //PasteMode value for gamesave backup from MC
#define PM_MC_RESTORE 2 //PasteMode value for gamesave restore to MC
#define MAX_RECURSE  16 //Maximum folder recursion for MC Backup/Restore

int PasteMode;            //Top-level PasteMode flag
int PM_flag[MAX_RECURSE]; //PasteMode flag for each 'copy' recursion level
int PM_file[MAX_RECURSE]; //PasteMode attribute file descriptors
int PM_fXio[MAX_RECURSE]; //Flags fileXio device usage for PasteMode attribute files
int PM_Indx[MAX_RECURSE]; //Attribute entry index for PasteMode attribute files

unsigned char *elisaFnt=NULL;
size_t freeSpace;
int mcfreeSpace;
int vfreeSpace;
int browser_cut;
int nclipFiles, nmarks, nparties;
int title_show;
int title_sort;
char mountedParty[2][MAX_NAME];
char parties[MAX_PARTITIONS][MAX_NAME];
char clipPath[MAX_PATH], LastDir[MAX_NAME], marks[MAX_ENTRY];
FILEINFO clipFiles[MAX_ENTRY];
int fileMode =  FIO_S_IRUSR | FIO_S_IWUSR | FIO_S_IXUSR | FIO_S_IRGRP | FIO_S_IWGRP | FIO_S_IXGRP | FIO_S_IROTH | FIO_S_IWOTH | FIO_S_IXOTH;

int host_ready   = 0;
int host_error   = 0;
int host_elflist = 0;
int host_use_Bsl = 1;	//By default assume that host paths use backslash
//--------------------------------------------------------------
//executable code
//--------------------------------------------------------------
void clear_mcTable(mcTable *mcT)
{
	memset((void *) mcT, 0, sizeof(mcTable));
}
//--------------------------------------------------------------
int getHddParty(const char *path, const FILEINFO *file, char *party, char *dir)
{
	char fullpath[MAX_PATH], *p;
	
	if(strncmp(path,"hdd",3)) return -1;
	
	strcpy(fullpath, path);
	if(file!=NULL){
		strcat(fullpath, file->name);
		if(file->stats.attrFile & MC_ATTR_SUBDIR) strcat(fullpath,"/");
	}
	if((p=strchr(&fullpath[6], '/'))==NULL) return -1;
	if(dir!=NULL) sprintf(dir, "pfs0:%s", p);
	*p=0;
	if(party!=NULL) sprintf(party, "hdd0:%s", &fullpath[6]);
	
	return 0;
}
//--------------------------------------------------------------
int mountParty(const char *party)
{
	if(!strcmp(party, mountedParty[0]))
		return 0;
	else if(!strcmp(party, mountedParty[1]))
		return 1;
	
	fileXioUmount("pfs0:"); mountedParty[0][0]=0;
	if(fileXioMount("pfs0:", party, FIO_MT_RDWR) < 0) return -1;
	strcpy(mountedParty[0], party);
	return 0;
}
//--------------------------------------------------------------
int ynDialog(const char *message)
{
	char msg[512];
	int dh, dw, dx, dy;
	int sel=0, a=6, b=4, c=2, n, tw;
	int i, x, len, ret;
	
	strcpy(msg, message);
	
	for(i=0,n=1; msg[i]!=0; i++){
		if(msg[i]=='\n'){
			msg[i]=0;
			n++;
		}
	}
	for(i=len=tw=0; i<n; i++){
		ret = printXY(&msg[len], 0, 0, 0, FALSE);
		if(ret>tw) tw=ret;
		len=strlen(&msg[len])+1;
	}
	if(tw<96) tw=96;
	
	dh = 16*(n+1)+2*2+a+b+c;
	dw = 2*2+a*2+tw;
	dx = (512-dw)/2;
	dy = (432-dh)/2;
	printf("tw=%d\ndh=%d\ndw=%d\ndx=%d\ndy=%d\n", tw,dh,dw,dx,dy);
	
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_LEFT){
				sel=0;
			}else if(new_pad & PAD_RIGHT){
				sel=1;
			}else if((!swapKeys && new_pad & PAD_CROSS)
			      || (swapKeys && new_pad & PAD_CIRCLE) ){
				ret=-1;
				break;
			}else if((swapKeys && new_pad & PAD_CROSS)
			      || (!swapKeys && new_pad & PAD_CIRCLE) ){
				if(sel==0) ret=1;
				else	   ret=-1;
				break;
			}
		}
		
		itoSprite(setting->color[0], 0, (SCREEN_MARGIN+FONT_HEIGHT+4)/2,
		SCREEN_WIDTH, (SCREEN_MARGIN+FONT_HEIGHT+4+FONT_HEIGHT)/2, 0);
		itoSprite(setting->color[0], dx-2, (dy-2)/2, dx+dw+2, (dy+dh+4)/2, 0);
		drawFrame(dx, dy/2, dx+dw, (dy+dh)/2, setting->color[1]);
		for(i=len=0; i<n; i++){
			printXY(&msg[len], dx+2+a,(dy+a+2+i*16)/2, setting->color[3],TRUE);
			len=strlen(&msg[len])+1;
		}
		x=(tw-96)/2;
		printXY(" OK   CANCEL", dx+a+x, (dy+a+b+2+n*16)/2, setting->color[3],TRUE);
		if(sel==0)
			drawChar(127, dx+a+x,(dy+a+b+2+n*16)/2, setting->color[3]);
		else
			drawChar(127,dx+a+x+40,(dy+a+b+2+n*16)/2,setting->color[3]);
		drawScr();
	}
	x=(tw-96)/2;
	drawChar(' ', dx+a+x,(dy+a+b+2+n*16)/2, setting->color[3]);
	drawChar(' ',dx+a+x+40,(dy+a+b+2+n*16)/2,setting->color[3]);
	return ret;
}
//--------------------------------------------------------------
int cmpFile(FILEINFO *a, FILEINFO *b)  //Used for directory sort
{
	unsigned char *p, ca, cb;
	int i, n, ret, aElf=FALSE, bElf=FALSE, t=(title_show & title_sort);
	
	if((a->stats.attrFile & MC_ATTR_OBJECT) == (b->stats.attrFile & MC_ATTR_OBJECT)){
		if(a->stats.attrFile & MC_ATTR_FILE){
			p = strrchr(a->name, '.');
			if(p!=NULL && !stricmp(p+1, "ELF")) aElf=TRUE;
			p = strrchr(b->name, '.');
			if(p!=NULL && !stricmp(p+1, "ELF")) bElf=TRUE;
			if(aElf && !bElf)		return -1;
			else if(!aElf && bElf)	return 1;
			t=FALSE;
		}
		if(t){
			if(a->title[0]!=0 && b->title[0]==0) return -1;
			else if(a->title[0]==0 && b->title[0]!=0) return 1;
			else if(a->title[0]==0 && b->title[0]==0) t=FALSE;
		}
		if(t) n=strlen(a->title);
		else  n=strlen(a->name);
		for(i=0; i<=n; i++){
			if(t){
				ca=a->title[i]; cb=b->title[i];
			}else{
				ca=a->name[i]; cb=b->name[i];
				if(ca>='a' && ca<='z') ca-=0x20;
				if(cb>='a' && cb<='z') cb-=0x20;
			}
			ret = ca-cb;
			if(ret!=0) return ret;
		}
		return 0;
	}
	
	if(a->stats.attrFile & MC_ATTR_SUBDIR)	return -1;
	else						return 1;
}
//--------------------------------------------------------------
void sort(FILEINFO *a, int left, int right) {
	FILEINFO tmp, pivot;
	int i, p;
	
	if (left < right) {
		pivot = a[left];
		p = left;
		for (i=left+1; i<=right; i++) {
			if (cmpFile(&a[i],&pivot)<0){
				p=p+1;
				tmp=a[p];
				a[p]=a[i];
				a[i]=tmp;
			}
		}
		a[left] = a[p];
		a[p] = pivot;
		sort(a, left, p-1);
		sort(a, p+1, right);
	}
}
//--------------------------------------------------------------
int readMC(const char *path, FILEINFO *info, int max)
{
	static mcTable mcDir[MAX_ENTRY] __attribute__((aligned(64)));
	char dir[MAX_PATH];
	int i, j, ret;
	
	mcSync(0,NULL,NULL);
	
	strcpy(dir, &path[4]); strcat(dir, "*");
	mcGetDir(path[2]-'0', 0, dir, 0, MAX_ENTRY-2, mcDir);
	mcSync(0, NULL, &ret);
	
	for(i=j=0; i<ret; i++)
	{
		if(mcDir[i].attrFile & MC_ATTR_SUBDIR &&
		(!strcmp(mcDir[i].name,".") || !strcmp(mcDir[i].name,"..")))
			continue;  //Skip pseudopaths "." and ".."
		strcpy(info[j].name, mcDir[i].name);
		info[j].stats = mcDir[i];
		j++;
	}
	
	return j;
}
//--------------------------------------------------------------
int readCD(const char *path, FILEINFO *info, int max)
{
	static struct TocEntry TocEntryList[MAX_ENTRY];
	char dir[MAX_PATH];
	int i, j, n;
	
	loadCdModules();
	
	strcpy(dir, &path[5]);
	CDVD_FlushCache();
	n = CDVD_GetDir(dir, NULL, CDVD_GET_FILES_AND_DIRS, TocEntryList, MAX_ENTRY, dir);
	
	for(i=j=0; i<n; i++)
	{
		if(TocEntryList[i].fileProperties & 0x02 &&
		 (!strcmp(TocEntryList[i].filename,".") ||
		  !strcmp(TocEntryList[i].filename,"..")))
			continue;  //Skip pseudopaths "." and ".."
		strcpy(info[j].name, TocEntryList[i].filename);
		clear_mcTable(&info[j].stats);
		if(TocEntryList[i].fileProperties & 0x02)
			info[j].stats.attrFile = MC_ATTR_norm_folder;
		else
			info[j].stats.attrFile = MC_ATTR_norm_file;
		j++;
	}
	
	return j;
}
//--------------------------------------------------------------
void setPartyList(void)
{
	iox_dirent_t dirEnt;
	int hddFd;
	
	nparties=0;
	
	if((hddFd=fileXioDopen("hdd0:")) < 0)
		return;
	while(fileXioDread(hddFd, &dirEnt) > 0)
	{
		if(nparties >= MAX_PARTITIONS)
			break;
		if((dirEnt.stat.attr & ATTR_SUB_PARTITION) 
				|| (dirEnt.stat.mode == FS_TYPE_EMPTY))
			continue;
//		if(!strncmp(dirEnt.name, "PP.HDL.", 7))  //Old test of partition type by 'name'
		if(dirEnt.stat.mode == 0x1337)  //New test of partition type by 'mode'
			continue;
		if(!strncmp(dirEnt.name, "__", 2) && strcmp(dirEnt.name, "__boot"))
			continue;
		
		strcpy(parties[nparties++], dirEnt.name);
	}
	fileXioDclose(hddFd);
}
//--------------------------------------------------------------
int readHDD(const char *path, FILEINFO *info, int max)
{
	iox_dirent_t dirbuf;
	char party[MAX_PATH], dir[MAX_PATH];
	int i=0, fd, ret;
	
	if(nparties==0){
		loadHddModules();
		setPartyList();
	}
	
	if(!strcmp(path, "hdd0:/")){
		for(i=0; i<nparties; i++){
			strcpy(info[i].name, parties[i]);
			info[i].stats.attrFile = MC_ATTR_norm_folder;
		}
		return nparties;
	}
	
	getHddParty(path,NULL,party,dir);
	ret = mountParty(party);
	if(ret<0) return 0;
	dir[3] = ret+'0';
	
	if((fd=fileXioDopen(dir)) < 0) return 0;
	
	while(fileXioDread(fd, &dirbuf)){
		if(dirbuf.stat.mode & FIO_S_IFDIR &&
		(!strcmp(dirbuf.name,".") || !strcmp(dirbuf.name,"..")))
			continue;  //Skip pseudopaths "." and ".."
		
		clear_mcTable(&info[i].stats);
		if(dirbuf.stat.mode & FIO_S_IFDIR)
			info[i].stats.attrFile = MC_ATTR_norm_folder;
		else if(dirbuf.stat.mode & FIO_S_IFDIR)
			info[i].stats.attrFile = MC_ATTR_norm_file;
		strcpy(info[i].name, dirbuf.name);
		i++;
		if(i==max) break;
	}
	
	fileXioDclose(fd);
	
	return i;
}
//--------------------------------------------------------------
int readMASS(const char *path, FILEINFO *info, int max)
{
	fat_dir_record record;
	int ret, n=0;
	
	loadUsbModules();
	
	ret = usb_mass_getFirstDirentry((char*)path+5, &record);
	while(ret > 0){
		if(record.attr & 0x10 &&
		(!strcmp(record.name,".") || !strcmp(record.name,".."))){
			ret = usb_mass_getNextDirentry(&record);
			continue;
		}
		
		strcpy(info[n].name, record.name);
		clear_mcTable(&info[n].stats);
		if(record.attr & 0x10)
			info[n++].stats.attrFile = MC_ATTR_norm_folder;
		else if(record.attr & 0x20)
			info[n++].stats.attrFile = MC_ATTR_norm_file;
		ret = usb_mass_getNextDirentry(&record);
	}
	
	return n;
}
//--------------------------------------------------------------
char	*makeHostPath(char *dp, char *sp)
{
	int	i;
	char ch;

	if	(!host_use_Bsl)
		return strcpy(dp, sp);

	for (i=0; i<MAX_PATH-1; i++)
	{	ch = sp[i];
		if	(ch == '/')
			dp[i] = '\\';
		else
			dp[i] = ch;
		if	(!ch)
			break;
	}
	return dp;
}
//--------------------------------------------------------------
char	*makeFslPath(char *dp, char *sp)
{
	int	i;
	char ch;

	for (i=0; i<MAX_PATH-1; i++)
	{	ch = sp[i];
		if	(ch == '\\')
			dp[i] = '/';
		else
			dp[i] = ch;
		if	(!ch)
			break;
	}
	return dp;
}
//--------------------------------------------------------------
void	initHOST(void)
{
	int	fd;

	load_ps2host();
	host_error = 0;
	if ((fd = fioOpen("host:elflist.txt", O_RDONLY)) >= 0)
	{ fioClose(fd);
		host_elflist = 1;
	}
	else
	{	host_elflist = 0;
		if ((fd = fioDopen("host:")) >= 0)
			fioDclose(fd);
		else
			host_error = 1;
	}
	host_ready = 1;
}
//--------------------------------------------------------------
int	readHOST(const char *path, FILEINFO *info, int max)
{
	fio_dirent_t hostcontent;
	int hfd, tfd, rv, size, contentptr, hostcount = 0;
	char *elflisttxt, elflistchar;
	char	host_path[MAX_PATH], host_next[MAX_PATH];
	char	Win_path[MAX_PATH];

	initHOST();
	snprintf(host_path, MAX_PATH-1, "%s", path);
	if	(!strncmp(path, "host:/", 6))
		strcpy(host_path+5, path+6);
	if	((host_elflist) && !strcmp(host_path, "host:")){
		if	((hfd = fioOpen("host:elflist.txt", O_RDONLY)) < 0)
			return 0;
		if	((size = fioLseek(hfd, 0, SEEK_END)) <= 0)
		{	fioClose(hfd);
			return 0;
		}
		elflisttxt = (char *)malloc(size);
		fioLseek(hfd, 0, SEEK_SET);
		fioRead(hfd, elflisttxt, size);
		fioClose(hfd);
		contentptr = 0;
		for (rv=0;rv<=size;rv++)
		{	elflistchar = elflisttxt[rv];
			if	((elflistchar == 0x0a) || (rv == size))
			{	host_next[contentptr] = 0;
				snprintf(host_path, MAX_PATH-1, "%s%s", "host:", host_next);
				clear_mcTable(&info[hostcount].stats);
				if	((hfd = fioOpen(makeHostPath(Win_path, host_path), O_RDONLY)) >= 0)
				{	fioClose(hfd);
					info[hostcount].stats.attrFile = MC_ATTR_norm_file;
					makeFslPath(info[hostcount++].name, host_next);
				} else if ((hfd = fioDopen(Win_path)) >= 0)
				{	fioDclose(hfd);
					info[hostcount].stats.attrFile = MC_ATTR_norm_folder;
					makeFslPath(info[hostcount++].name, host_next);
				}
				contentptr = 0;
				if	(hostcount > max)
					break;
			}
			else if	(elflistchar != 0x0d)
			{	host_next[contentptr]=elflistchar;
				contentptr++;
			}
		}
		free(elflisttxt);
		return hostcount-1;
	}	//This point is only reached if elflist.txt is NOT to be used
	if ((hfd = fioDopen(makeHostPath(Win_path, host_path))) < 0)
		return 0;
	strcpy(host_path, Win_path);
	while ((rv = fioDread(hfd, &hostcontent)))
	{	if (strcmp(hostcontent.name, ".")&&strcmp(hostcontent.name, ".."))
		{
			snprintf(Win_path, MAX_PATH-1, "%s%s", host_path, hostcontent.name);
			strcpy(info[hostcount].name, hostcontent.name);
			clear_mcTable(&info[hostcount].stats);
			if	((tfd = fioOpen(Win_path, O_RDONLY)) >= 0)
			{	fioClose(tfd);
				info[hostcount++].stats.attrFile = MC_ATTR_norm_file;
			} else if ((tfd = fioDopen(Win_path)) >= 0)
			{	fioDclose(tfd);
				info[hostcount++].stats.attrFile = MC_ATTR_norm_folder;
			}
			if (hostcount >= max)
				break;
		}
	}
	fioDclose(hfd);
	strcpy (info[hostcount].name, "\0");
	return hostcount;
}
//--------------------------------------------------------------
int getDir(const char *path, FILEINFO *info)
{
	int max=MAX_ENTRY-2;
	int n;
	
	if(!strncmp(path, "mc", 2))			n=readMC(path, info, max);
	else if(!strncmp(path, "hdd", 3))	n=readHDD(path, info, max);
	else if(!strncmp(path, "mass", 4))	n=readMASS(path, info, max);
	else if(!strncmp(path, "cdfs", 4))	n=readCD(path, info, max);
	else if(!strncmp(path, "host", 4))	n=readHOST(path, info, max);
	else return 0;
	
	return n;
}
//--------------------------------------------------------------
int getGameTitle(const char *path, const FILEINFO *file, char *out)
{
	iox_dirent_t dirEnt;
	char party[MAX_NAME], dir[MAX_PATH];
	int fd=-1, dirfd=-1, size, hddin=FALSE, ret;
	
	if(file->stats.attrFile & MC_ATTR_FILE) return -1;
	if(path[0]==0 || !strcmp(path,"hdd0:/")) return -1;
	
	if(!strncmp(path, "hdd", 3)){
		ret = getHddParty(path, file, party, dir);
		if(mountParty(party)<0) return -1;
		dir[3]=ret+'0';
		hddin=TRUE;
	}else
		sprintf(dir, "%s%s/", path, file->name);
	
	ret = -1;
	if(hddin){
		if((dirfd=fileXioDopen(dir)) < 0) goto error;
		while(fileXioDread(dirfd, &dirEnt)){
			if(dirEnt.stat.mode & FIO_S_IFREG &&
			 !strcmp(dirEnt.name,"icon.sys")){
				strcat(dir, "icon.sys");
				if((fd=fileXioOpen(dir, O_RDONLY, fileMode)) < 0)
					goto error;
				if((size=fileXioLseek(fd,0,SEEK_END)) <= 0x100)
					goto error;
				fileXioLseek(fd,0xC0,SEEK_SET);
				fileXioRead(fd, out, 16*4);
				out[16*4] = 0;
				fileXioClose(fd); fd=-1;
				ret=0;
				break;
			}
		}
		fileXioDclose(dirfd); dirfd=-1;
	}else{
		strcat(dir, "icon.sys");
		if((fd=fioOpen(dir, O_RDONLY)) < 0) goto error;
		if((size=fioLseek(fd,0,SEEK_END)) <= 0x100) goto error;
		fioLseek(fd,0xC0,SEEK_SET);
		fioRead(fd, out, 16*4);
		out[16*4] = 0;
		fioClose(fd); fd=-1;
		ret=0;
	}
error:
	if(fd>=0){
		if(hddin) fileXioClose(fd);
		else	  fioClose(fd);
	}
	if(dirfd>=0) fileXioDclose(dirfd);
	return ret;
}
//--------------------------------------------------------------
int menu(const char *path, const char *file)
{
	uint64 color;
	char enable[NUM_MENU], tmp[64];
	int x, y, i, sel;

	int menu_ch_w = 8;             //Total characters in longest menu string
	int menu_ch_h = NUM_MENU;      //Total number of menu lines
	int mSprite_Y1 = 32;           //Top edge of sprite
	int mSprite_X2 = 493;          //Right edge of sprite
	int mFrame_Y1 = mSprite_Y1+1;  //Top edge of frame
	int mFrame_X2 = mSprite_X2-3;  //Right edge of frame (-3 correct ???)
	int mFrame_X1 = mFrame_X2-(menu_ch_w+3)*FONT_WIDTH;    //Left edge of frame
	int mFrame_Y2 = mFrame_Y1+(menu_ch_h+1)*FONT_HEIGHT/2; //Bottom edge of frame
	int mSprite_X1 = mFrame_X1-1;  //Left edge of sprite
	int mSprite_Y2 = mFrame_Y2+2;  //Bottom edge of sprite (+2 correct ???)

	memset(enable, TRUE, NUM_MENU);
	if(!strncmp(path,"host",4)){
		enable[CUT] = FALSE;
		enable[PASTE] = FALSE;
		enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		enable[NEWDIR] = FALSE;
		enable[MCPASTE] = FALSE;
	}
	if(!strcmp(path,"hdd0:/") || path[0]==0){
		enable[COPY] = FALSE;
		enable[CUT] = FALSE;
		enable[PASTE] = FALSE;
		enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		enable[NEWDIR] = FALSE;
		enable[GETSIZE] = FALSE;
		enable[MCPASTE] = FALSE;
	}
	if(!strncmp(path,"cdfs",4)){
		enable[CUT] = FALSE;
		enable[PASTE] = FALSE;
		enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		enable[NEWDIR] = FALSE;
	}
	if(!strncmp(path, "mass", 4)){
		//enable[CUT] = FALSE;
		//enable[PASTE] = FALSE;
		//enable[DELETE] = FALSE;
		enable[RENAME] = FALSE;
		//enable[NEWDIR] = FALSE;
	}
	if(!strncmp(path, "mc", 2))
		enable[RENAME] = FALSE;
	if(nmarks==0){
		if(!strcmp(file, "..")){
			enable[COPY] = FALSE;
			enable[CUT] = FALSE;
			enable[DELETE] = FALSE;
			enable[RENAME] = FALSE;
			enable[GETSIZE] = FALSE;
		}
	}else
		enable[RENAME] = FALSE;
	if(nclipFiles==0){
		//Nothing in clipboard
		enable[PASTE] = FALSE;
		enable[MCPASTE] = FALSE;
	} else {
		//Something in clipboard
		if(!strncmp(path, "mc", 2)){
			if(!strncmp(clipPath, "mc", 2))
				enable[MCPASTE] = FALSE;  //No mcPaste if both src and dest are MC
		}	else
			if(strncmp(clipPath, "mc", 2))
				enable[MCPASTE] = FALSE;  //No mcPaste if both src and dest non-MC
	}
	for(sel=0; sel<NUM_MENU; sel++)
		if(enable[sel]==TRUE) break;
	
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad & PAD_UP && sel<NUM_MENU){
				do{
					sel--;
					if(sel<0) sel=NUM_MENU-1;
				}while(!enable[sel]);
			}else if(new_pad & PAD_DOWN && sel<NUM_MENU){
				do{
					sel++;
					if(sel==NUM_MENU) sel=0;
				}while(!enable[sel]);
			}else if((!swapKeys && new_pad & PAD_CROSS)
			      || (swapKeys && new_pad & PAD_CIRCLE) ){
				return -1;
			}else if((swapKeys && new_pad & PAD_CROSS)
			      || (!swapKeys && new_pad & PAD_CIRCLE) ){
				break;
			}
		}

		itoSprite(setting->color[0], mSprite_X1, mSprite_Y1, mSprite_X2, mSprite_Y2, 0);
		drawFrame(mFrame_X1, mFrame_Y1, mFrame_X2, mFrame_Y2, setting->color[1]);
		
		for(i=0,y=mFrame_Y1*2+FONT_HEIGHT/2; i<NUM_MENU; i++){
			if(i==COPY)			strcpy(tmp, "Copy");
			else if(i==CUT)		strcpy(tmp, "Cut");
			else if(i==PASTE)	strcpy(tmp, "Paste");
			else if(i==DELETE)	strcpy(tmp, "Delete");
			else if(i==RENAME)	strcpy(tmp, "Rename");
			else if(i==NEWDIR)	strcpy(tmp, "New Dir");
			else if(i==GETSIZE) strcpy(tmp, "Get Size");
			else if(i==MCPASTE)	strcpy(tmp, "mcPaste");
			
			if(enable[i])	color = setting->color[3];
			else			color = setting->color[1];
			
			printXY(tmp, mFrame_X1+2*FONT_WIDTH, y/2, color, TRUE);
			y+=FONT_HEIGHT;
		}
		if(sel<NUM_MENU)
			drawChar(127, mFrame_X1+FONT_WIDTH, mFrame_Y1+(FONT_HEIGHT/2+sel*FONT_HEIGHT)/2, setting->color[3]);
		
		x = SCREEN_MARGIN;
		y = SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT;
		itoSprite(setting->color[0],
			0,
			y/2,
			SCREEN_WIDTH,
			y/2+8, 0);
		if (swapKeys)
			printXY("~:OK ›:Cancel", x, y/2, setting->color[2], TRUE);
		else
			printXY("›:OK ~:Cancel", x, y/2, setting->color[2], TRUE);
		drawScr();
	}
	
	return sel;
}
//--------------------------------------------------------------
size_t getFileSize(const char *path, const FILEINFO *file)
{
	size_t size;
	FILEINFO files[MAX_ENTRY];
	char dir[MAX_PATH], party[MAX_NAME];
	int nfiles, i, ret, fd;
	
	if(file->stats.attrFile & MC_ATTR_SUBDIR){
		sprintf(dir, "%s%s/", path, file->name);
		nfiles = getDir(dir, files);
		for(i=size=0; i<nfiles; i++){
			ret=getFileSize(dir, &files[i]);
			if(ret < 0) size = -1;
			else		size+=ret;
		}
	} else {
		if(!strncmp(path, "hdd", 3)){
			getHddParty(path,file,party,dir);
			ret = mountParty(party);
			if(ret<0) return 0;
			dir[3] = ret+'0';
		}else
			sprintf(dir, "%s%s", path, file->name);
		if	(!strncmp(dir, "host:/", 6))
			makeHostPath(dir+5, dir+6);
		if(!strncmp(path, "hdd", 3)){
			fd = fileXioOpen(dir, O_RDONLY, fileMode);
			size = fileXioLseek(fd,0,SEEK_END);
			fileXioClose(fd);
		}else{
			fd = fioOpen(dir, O_RDONLY);
			size = fioLseek(fd,0,SEEK_END);
			fioClose(fd);;
		}
	}
	return size;
}
//--------------------------------------------------------------
int delete(const char *path, const FILEINFO *file)
{
	FILEINFO files[MAX_ENTRY];
	char party[MAX_NAME], dir[MAX_PATH], hdddir[MAX_PATH];
	int nfiles, i, ret;
	
	if(!strncmp(path, "hdd", 3)){
		getHddParty(path,file,party,hdddir);
		ret = mountParty(party);
		if(ret<0) return 0;
		hdddir[3] = ret+'0';
	}
	sprintf(dir, "%s%s", path, file->name);
	
	if(file->stats.attrFile & MC_ATTR_SUBDIR){
		strcat(dir,"/");
		nfiles = getDir(dir, files);
		for(i=0; i<nfiles; i++){
			ret=delete(dir, &files[i]);
			if(ret < 0) return -1;
		}
		if(!strncmp(dir, "mc", 2)){
			mcSync(0,NULL,NULL);
			mcDelete(dir[2]-'0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
		}else if(!strncmp(path, "hdd", 3)){
			ret = fileXioRmdir(hdddir);
		}else if(!strncmp(path, "mass", 4)){
			sprintf(dir, "mass0:%s%s", &path[5], file->name);
			ret = fioRmdir(dir);
			if (ret < 0){
				dir[4] = 1 + '0';
				ret = fioRmdir(dir);
			}
		}
	} else {
		if(!strncmp(path, "mc", 2)){
			mcSync(0,NULL,NULL);
			mcDelete(dir[2]-'0', 0, &dir[4]);
			mcSync(0, NULL, &ret);
		}else if(!strncmp(path, "hdd", 3)){
			ret = fileXioRemove(hdddir);
		}else if(!strncmp(path, "mass", 4)){
			ret = fioRemove(dir);
		}
	}
	return ret;
}
//--------------------------------------------------------------
int Rename(const char *path, const FILEINFO *file, const char *name)
{
	char party[MAX_NAME], oldPath[MAX_PATH], newPath[MAX_PATH];
	int ret=0;
	
	if(!strncmp(path, "hdd", 3)){
		sprintf(party, "hdd0:%s", &path[6]);
		*strchr(party, '/')=0;
		sprintf(oldPath, "pfs0:%s", strchr(&path[6], '/')+1);
		sprintf(newPath, "%s%s", oldPath, name);
		strcat(oldPath, file->name);
		
		ret = mountParty(party);
		if(ret<0) return -1;
		oldPath[3] = newPath[3] = ret+'0';
		
		ret=fileXioRename(oldPath, newPath);
	}else if(!strncmp(path, "mc", 2)){
		sprintf(oldPath, "%s%s", path+2, file->name);
		sprintf(newPath, "%s%s", path+2, name);
		mcSync(0,NULL,NULL);
		mcRename(path[2]-'0', 0, oldPath, newPath);
		mcSync(0, NULL, &ret);
		if(ret == -4)
			ret = -17;
	}else
		return -1;
		
	return ret;
}

//--------------------------------------------------------------
int newdir(const char *path, const char *name)
{
	char party[MAX_NAME], dir[MAX_PATH];
	int ret=0;
	
	if(!strncmp(path, "hdd", 3)){
		getHddParty(path,NULL,party,dir);
		ret = mountParty(party);
		if(ret<0) return -1;
		dir[3] = ret+'0';
		//fileXioChdir(dir);
		strcat(dir, name);
		ret = fileXioMkdir(dir, fileMode);
	}else if(!strncmp(path, "mc", 2)){
		sprintf(dir, "%s%s", path+4, name);
		mcSync(0,NULL,NULL);
		mcMkDir(path[2]-'0', 0, dir);
		mcSync(0, NULL, &ret);
		if(ret == -4)
			ret = -17;
	}else if(!strncmp(path, "mass", 4)){
		strcpy(dir, path);
		strcat(dir, name);
		ret = fioMkdir(dir);
	}
	
	return ret;
}
//--------------------------------------------------------------
//The function 'copy' below is called to copy a single object,
//indicated by 'file' from 'inPath' to 'outPath', but this may
//be either a single file or a folder. In the latter case the
//folder contents should also be copied, recursively.
//--------------------------------------------------------------
int copy(const char *outPath, const char *inPath, FILEINFO file, int recurses)
{
	FILEINFO files[MAX_ENTRY];
	char out[MAX_PATH], in[MAX_PATH], tmp[MAX_PATH],
		*buff=NULL, inParty[MAX_NAME], outParty[MAX_NAME];
	int hddout=FALSE, hddin=FALSE, nfiles, i;
	size_t size, outsize;
	int ret=-1, pfsout=-1, pfsin=-1, in_fd=-1, out_fd=-1, buffSize;
	int dummy;
	mcTable stats;

	sprintf(out, "%s%s", outPath, file.name);
	sprintf(in, "%s%s", inPath, file.name);
	
	if(!strncmp(inPath, "hdd", 3)){
		hddin = TRUE;
		getHddParty(inPath, &file, inParty, in);
		if(!strcmp(inParty, mountedParty[0]))
			pfsin=0;
		else if(!strcmp(inParty, mountedParty[1]))
			pfsin=1;
		else
			pfsin=-1;
	}
	if(!strncmp(outPath, "hdd", 3)){
		hddout = TRUE;
		getHddParty(outPath, &file, outParty, out);
		if(!strcmp(outParty, mountedParty[0]))
			pfsout=0;
		else if(!strcmp(outParty, mountedParty[1]))
			pfsout=1;
		else
			pfsout=-1;
	}
	if(hddin){
		if(pfsin<0){
			if(pfsout==0) pfsin=1;
			else		  pfsin=0;
			sprintf(tmp, "pfs%d:", pfsin);
			if(mountedParty[pfsin][0]!=0)
				fileXioUmount(tmp); mountedParty[pfsin][0]=0;
			printf("%s mounting\n", inParty);
			if(fileXioMount(tmp, inParty, FIO_MT_RDWR) < 0) return -1;
			strcpy(mountedParty[pfsin], inParty);
		}
		in[3]=pfsin+'0';
	}else
		sprintf(in, "%s%s", inPath, file.name);
	if(hddout){
		if(pfsout<0){
			if(pfsin==0) pfsout=1;
			else		 pfsout=0;
			sprintf(tmp, "pfs%d:", pfsout);
			if(mountedParty[pfsout][0]!=0)
				fileXioUmount(tmp); mountedParty[pfsout][0]=0;
			if(fileXioMount(tmp, outParty, FIO_MT_RDWR) < 0) return -1;
			printf("%s mounting\n", outParty);
			strcpy(mountedParty[pfsout], outParty);
		}
		out[3]=pfsout+'0';
	}else
		sprintf(out, "%s%s", outPath, file.name);
//Here 'in' and 'out' are complete pathnames for the object to copy
//patched to contain appropriate 'pfs' refs where args used 'hdd'
//The physical device specifiers remain in 'inPath' and 'outPath'

//Here we have an object to copy, which may be either file or folder
	if(file.stats.attrFile & MC_ATTR_SUBDIR){
//Here we have a folder to copy, starting with an attempt to create it
		ret = newdir(outPath, file.name);
		if(ret == -17){
			ret=-1;
			if(title_show) ret=getGameTitle(outPath, &file, tmp);
			if(ret<0) sprintf(tmp, "%s%s/", outPath, file.name);
			strcat(tmp, "\nOverwrite?");
			if(ynDialog(tmp)<0) return -1;
			drawMsg("Pasting...");
		} else if(ret < 0)
			return -1;
//The return code above is for failure to create destination folder
//Here a destination folder exists, ready to receive files
		PM_flag[recurses+1] = PM_NORMAL;
		if(PasteMode==PM_MC_BACKUP){         //MC Backup mode folder paste preparation
			sprintf(tmp, "%s/PS2_MC_Backup_Attributes.BUP.bin", out);

			if(hddout){
				PM_fXio[recurses+1] = 1;
				fileXioRemove(tmp);
				out_fd = fileXioOpen(tmp,O_WRONLY|O_TRUNC|O_CREAT,fileMode);
			} else {
				PM_fXio[recurses+1] = 0;
				if	(!strncmp(tmp, "host:/", 6))
					makeHostPath(tmp+5, tmp+6);
				out_fd=fioOpen(tmp, O_WRONLY | O_TRUNC | O_CREAT);
			}

			if(out_fd>=0){
				if(PM_fXio[recurses+1]) size = fileXioWrite(out_fd, (void *) &file.stats, 64);
				else                    size = fioWrite(out_fd, (void *) &file.stats, 64);
				if(size == 64){
					PM_file[recurses+1] = out_fd;
					PM_flag[recurses+1] = PM_MC_BACKUP;
				}
				else
					if(PM_fXio[recurses+1]) fileXioClose(PM_file[recurses+1]);
					else                        fioClose(PM_file[recurses+1]);
			}
		} else if(PasteMode==PM_MC_RESTORE){ //MC Restore mode folder paste preparation
			sprintf(tmp, "%s/PS2_MC_Backup_Attributes.BUP.bin", in);

			if(hddin){
				PM_fXio[recurses+1] = 1;
				in_fd = fileXioOpen(tmp, O_RDONLY, fileMode);
			} else{
				PM_fXio[recurses+1] = 0;
				if	(!strncmp(tmp, "host:/", 6))
					makeHostPath(tmp+5, tmp+6);
				in_fd = fioOpen(tmp, O_RDONLY);
			}

			if(in_fd>=0){
				if(PM_fXio[recurses+1]) size = fileXioRead(in_fd, (void *) &file.stats, 64);
				else	                  size = fioRead(in_fd, (void *) &file.stats, 64);
				if(size == 64){
					PM_file[recurses+1] = in_fd;
					PM_flag[recurses+1] = PM_MC_RESTORE;
				}
				else
					if(PM_fXio[recurses+1]) fileXioClose(PM_file[recurses+1]);
					else                        fioClose(PM_file[recurses+1]);
			}
		}
		if(PM_flag[recurses+1]==PM_NORMAL){  //Normal mode folder paste preparation
		}
		sprintf(out, "%s%s/", outPath, file.name);  //restore phys dev spec to 'out'
		sprintf(in, "%s%s/", inPath, file.name);    //restore phys dev spec to 'in'

		nfiles = getDir(in, files);
		for(i=0; i<nfiles; i++)
			if((ret = copy(out, in, files[i], recurses+1)) < 0) break;
//folder contents are copied by the recursive call above, with error handling below
		if(ret<0) return -1;

//Here folder contents have been copied error-free, but we also need to copy
//attributes and timestamps, and close the attribute file if such was used
		if(PM_flag[recurses+1]==PM_MC_BACKUP){         //MC Backup mode folder paste closure
			if(PM_fXio[recurses+1]) fileXioClose(PM_file[recurses+1]);
			else                        fioClose(PM_file[recurses+1]);
		} else if(PM_flag[recurses+1]==PM_MC_RESTORE){ //MC Restore mode folder paste closure
			in_fd = PM_file[recurses+1];
			for(size=64, i=0; size==64;){
				if(PM_fXio[recurses+1]) size = fileXioRead(in_fd, (void *) &stats, 64);
				else	                  size = fioRead(in_fd, (void *) &stats, 64);
				if(size == 64){
					strcpy(tmp, out);
					strncat(tmp, stats.name, 32);
					mcGetInfo(tmp[2]-'0', 0, &dummy, &dummy, &dummy);  //Wakeup call
					mcSync(0, NULL, &dummy);
					mcSetFileInfo(tmp[2]-'0', 0, &tmp[4], &stats, 0xFFFF); //Fix file stats
					mcSync(0, NULL, &dummy);
				} else {
					if(PM_fXio[recurses+1]) fileXioClose(in_fd);
					else                        fioClose(in_fd);
				}
			}
			//Finally fix the stats of the containing folder
			//It has to be done last, as timestamps would change when fixing files
			mcGetInfo(out[2]-'0', 0, &dummy, &dummy, &dummy);  //Wakeup call
			mcSync(0, NULL, &dummy);
			mcSetFileInfo(out[2]-'0', 0, &out[4], &file.stats, 0xFFFF); //Fix folder stats
			mcSync(0, NULL, &dummy);
		} else if(PM_flag[recurses+1]==PM_NORMAL){     //Normal mode folder paste closure
			if(!strncmp(out, "mc", 2)){  //Handle folder copied to MC
				mcGetInfo(out[2]-'0', 0, &dummy, &dummy, &dummy);  //Wakeup call
				mcSync(0, NULL, &dummy);
				if(!strncmp(in, "mc", 2)){  //Handle folder copied from MC to MC
					//For this case we set the entire mcTable struct like the original
					mcSetFileInfo(out[2]-'0', 0, &out[4], &file.stats, 0xFFFF);
					mcSync(0, NULL, &dummy);
				} else {  //Handle folder copied from non-MC to MC
					//For the present we only set the standard folder attributes here
					file.stats.attrFile = MC_ATTR_norm_folder;
					mcSetFileInfo(out[2]-'0', 0, &out[4], &file.stats, 4);
					mcSync(0, NULL, &dummy);
				}
			} else {  //Handle folder copied to non-MC
				if(!strncmp(in, "mc", 2)){  //Handle folder copied from MC to non-MC
					//For the present this case is ignored (new timestamps and attr)
				} else {  //Handle folder copied from non-MC to non-MC
					//For the present this case is ignored (new timestamps and attr)
				}
			}
		}
//the return code below is used if there were no errors copying a folder
		return 0;
	}

//Here we know that the object to copy is a file, not a folder
//In MC Restore mode we must here avoid copying the attribute file
	if(PM_flag[recurses]==PM_MC_RESTORE)
		if(!strcmp(file.name,"PS2_MC_Backup_Attributes.BUP.bin"))
			return 0;

//It is now time to open the input file, indicated by 'in_fd'
	if(hddin){
		in_fd = fileXioOpen(in, O_RDONLY, fileMode);
		if(in_fd<0) goto error;
		size = fileXioLseek(in_fd,0,SEEK_END);
		fileXioLseek(in_fd,0,SEEK_SET);
	}else{
		if	(!strncmp(in, "host:/", 6))
			makeHostPath(in+5, in+6);
		in_fd = fioOpen(in, O_RDONLY);
		if(in_fd<0) goto error;
		size = fioLseek(in_fd,0,SEEK_END);
		fioLseek(in_fd,0,SEEK_SET);
	}

//Here the input file has been opened, indicated by 'in_fd'
//It is now time to open the output file, indicated by 'out_fd'
		if(hddout){
			fileXioRemove(out);
			out_fd = fileXioOpen(out,O_WRONLY|O_TRUNC|O_CREAT,fileMode);
			if(out_fd<0) goto error;
		}else{
			out_fd=fioOpen(out, O_WRONLY | O_TRUNC | O_CREAT);
			if(out_fd<0) goto error;
		}
//Here the output file has been opened, indicated by 'out_fd'

	buff = (char*)malloc(size);
	if(buff==NULL){
		buff = (char*)malloc(32768);
		buffSize = 32768;
	}else
		buffSize = size;
	while(size>0){
		if(hddin) buffSize = fileXioRead(in_fd, buff, buffSize);
		else	  buffSize = fioRead(in_fd, buff, buffSize);
		if(hddout){
			outsize = fileXioWrite(out_fd,buff,buffSize);
			if(buffSize!=outsize){
				fileXioClose(out_fd); out_fd=-1;
				fileXioRemove(out);
				goto error;
			}
		}else{
			outsize = fioWrite(out_fd,buff,buffSize);
			if(buffSize!=outsize){
				FILEINFO *FI_p = &file;
				fioClose(out_fd); out_fd=-1;
				delete(out, FI_p);
				goto error;
			}
		}
		size -= buffSize;
	}
	ret=0;
//Here the file has been copied. without error, as indicated by 'ret' above
//but we also need to copy attributes and timestamps (as yet only for MC)
	if(PM_flag[recurses]==PM_MC_BACKUP){         //MC Backup mode file paste closure
		if(PM_fXio[recurses]) size = fileXioWrite(PM_file[recurses], (void *) &file.stats, 64);
		else                  size = fioWrite(PM_file[recurses], (void *) &file.stats, 64);
		if(size != 64) return -1; //Abort if attribute file crashed
	}
	if(!strncmp(out, "mc", 2)){  //Handle file copied to MC
		mcGetInfo(out[2]-'0', 0, &dummy, &dummy, &dummy);  //Wakeup call
		mcSync(0, NULL, &dummy);
		if(!strncmp(in, "mc", 2)){  //Handle file copied from MC to MC
			//For this case we set the entire mcTable struct like the original
			mcSetFileInfo(out[2]-'0', 0, &out[4], &file.stats, 0xFFFF);
			mcSync(0, NULL, &dummy);
		} else {  //Handle file copied from non-MC to MC
			//For the present we only set the standard file attributes here
			file.stats.attrFile = MC_ATTR_norm_file;
			mcSetFileInfo(out[2]-'0', 0, &out[4], &file.stats, 4);
			mcSync(0, NULL, &dummy);
		}
	} else {  //Handle file copied to non-MC
		if(!strncmp(in, "mc", 2)){  //Handle file copied from MC to non-MC
			//For the present this case is ignored (new timestamps and attr)
		} else {  //Handle file copied from non-MC to non-MC
			//For the present this case is ignored (new timestamps and attr)
		}
	}
//The code below is also used for all errors in copying a file,
//but those cases are distinguished by a negative value in 'ret'
error:
	free(buff);
	if(in_fd>0){
		if(hddin) fileXioClose(in_fd);
		else	  fioClose(in_fd);
	}
	if(out_fd>0){
		if(hddout) fileXioClose(out_fd);
		else	  fioClose(out_fd);
	}
	return ret;
}
//--------------------------------------------------------------
int keyboard(char *out, int max)
{
	const int	KEY_W=276,
				KEY_H=84,
				KEY_X=130,
				KEY_Y=60,
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
	int cur=0, sel=0, i, x, y, t=0;
	char tmp[256], *p;
	
	p=strrchr(out, '.');
	if(p==NULL)	cur=strlen(out);
	else		cur=(int)(p-out);
	KEY_LEN = strlen(KEY);
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
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
				if(sel < WFONTS*HFONTS){
					if(i<max && i<33){
						strcpy(tmp, out);
						out[cur]=KEY[sel];
						out[cur+1]=0;
						strcat(out, &tmp[cur]);
						cur++;
						t=0;
					}
				}else if(sel == WFONTS*HFONTS && i>0){
					break;
				}else
					return -1;
			}
		}
		// •`‰æŠJŽn
		itoSprite(setting->color[0],
			KEY_X-2,
			KEY_Y-1,
			KEY_X+KEY_W+3,
			KEY_Y+KEY_H+2, 0);
		drawFrame(
			KEY_X,
			KEY_Y,
			KEY_X+KEY_W,
			KEY_Y+KEY_H,setting->color[1]);
		itoLine(setting->color[1], KEY_X, KEY_Y+11, 0,
			setting->color[1], KEY_X+KEY_W, KEY_Y+11, 0);
		printXY(out,
			KEY_X+2+3,
			KEY_Y+2,
			setting->color[3], TRUE);
		t++;
		if(t<SCANRATE/2){
			printXY("|",
				KEY_X+cur*8+1,
				KEY_Y+2,
				setting->color[3], TRUE);
		}else{
			if(t==SCANRATE) t=0;
		}
		for(i=0; i<KEY_LEN; i++)
			drawChar(KEY[i],
				KEY_X+2+4 + (i%WFONTS+1)*20 - 12,
				KEY_Y+16 + (i/WFONTS)*8,
				setting->color[3]);
		printXY("OK                       CANCEL",
			KEY_X+2+4 + 20 - 12,
			KEY_Y+16 + HFONTS*8,
			setting->color[3], TRUE);
		if(sel<=WFONTS*HFONTS)
			x = KEY_X+2+4 + (sel%WFONTS+1)*20 - 20;
		else
			x = KEY_X+2+4 + 25*8;
		y = KEY_Y+16 + (sel/WFONTS)*8;
		drawChar(127, x, y, setting->color[3]);
		
		// ‘€ìà–¾
		x = SCREEN_MARGIN;
		y = SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT;
		itoSprite(setting->color[0],
			0,
			y/2,
			SCREEN_WIDTH,
			y/2+8, 0);
		if (swapKeys) 
			printXY("~:OK ›:Back L1:Left R1:Right START:Enter",x, y/2,
			        setting->color[2], TRUE);
		else
			printXY("›:OK ~:Back L1:Left R1:Right START:Enter",x, y/2,
			        setting->color[2], TRUE);
		drawScr();
	}
	return 0;
}
//--------------------------------------------------------------
int setFileList(const char *path, const char *ext, FILEINFO *files, int cnfmode)
{
	char *p;
	int nfiles, i, j, ret;
	
	if(path[0]==0){
		strcpy(files[0].name, "mc0:");
		strcpy(files[1].name, "mc1:");
		strcpy(files[2].name, "hdd0:");
		strcpy(files[3].name, "cdfs:");
		strcpy(files[4].name, "mass:");
		files[0].stats.attrFile = MC_ATTR_SUBDIR;
		files[1].stats.attrFile = MC_ATTR_SUBDIR;
		files[2].stats.attrFile = MC_ATTR_SUBDIR;
		files[3].stats.attrFile = MC_ATTR_SUBDIR;
		files[4].stats.attrFile = MC_ATTR_SUBDIR;
		nfiles = 5;
		if	(!cnfmode)
		{	strcpy(files[nfiles].name, "host:");
			files[nfiles++].stats.attrFile = MC_ATTR_SUBDIR;
		}
		for(i=0; i<nfiles; i++)
			files[i].title[0]=0;
		if(cnfmode){
			strcpy(files[nfiles].name, "MISC");
			files[nfiles].stats.attrFile = MC_ATTR_SUBDIR;
			nfiles++;
		}
		vfreeSpace=FALSE;
	}else if(!strcmp(path, "MISC/")){
		strcpy(files[0].name, "..");
		files[0].stats.attrFile = MC_ATTR_SUBDIR;
		strcpy(files[1].name, "FileBrowser");
		files[1].stats.attrFile = MC_ATTR_FILE;
		strcpy(files[2].name, "PS2Browser");
		files[2].stats.attrFile = MC_ATTR_FILE;
		strcpy(files[3].name, "PS2Disc");
		files[3].stats.attrFile = MC_ATTR_FILE;
		strcpy(files[4].name, "PS2Net");
		files[4].stats.attrFile = MC_ATTR_FILE;
		nfiles = 5;
//Next 5 line section is only for use while debugging
/*
		strcpy(files[5].name, "IOP Reset");
		files[5].stats.attrFile = MC_ATTR_FILE;
		strcpy(files[6].name, "Debug Screen");
		files[6].stats.attrFile = MC_ATTR_FILE;
		nfiles += 2;
*/
//End of section used only for debugging
		for(i=0; i<nfiles; i++)
			files[i].title[0]=0;
	}else{
		strcpy(files[0].name, "..");
		files[0].stats.attrFile = MC_ATTR_SUBDIR;
		nfiles = getDir(path, &files[1]) + 1;
		if(strcmp(ext,"*")){
			for(i=j=1; i<nfiles; i++){
				if(files[i].stats.attrFile & MC_ATTR_SUBDIR)
					files[j++] = files[i];
				else{
					p = strrchr(files[i].name, '.');
					if(p!=NULL && !stricmp(ext,p+1))
						files[j++] = files[i];
				}
			}
			nfiles = j;
		}
		if(title_show){
			for(i=1; i<nfiles; i++){
				ret = getGameTitle(path, &files[i], files[i].title);
				if(ret<0) files[i].title[0]=0;
			}
		}
		if(!strcmp(path, "hdd0:/"))
			vfreeSpace=FALSE;
		else if(nfiles>1)
			sort(&files[1], 0, nfiles-2);
	}
	
	return nfiles;
}

//--------------------------------------------------------------
// get_FilePath is the main browser function.
// It also contains the menu handler for the R1 submenu
// The static variables declared here are only for the use of
// this function and the submenu functions that it calls
//--------------------------------------------------------------
// sincro: ADD USBD_IRX_CNF mode for found IRX file for USBD.IRX
// example: getFilePath(setting->usbd, USBD_IRX_CNF);
static int browser_cd, browser_up, browser_pushed;
static int browser_sel, browser_nfiles;
static void submenu_func_GetSize(char *mess, char *path, FILEINFO *files);
static void submenu_func_Paste(char *mess, char *path);
static void submenu_func_mcPaste(char *mess, char *path);
void getFilePath(char *out, int cnfmode)
{
	char path[MAX_PATH], oldFolder[MAX_PATH],
		msg0[MAX_PATH], msg1[MAX_PATH],
		tmp[MAX_PATH], ext[8], *p;
	uint64 color;
	FILEINFO files[MAX_ENTRY];
	int top=0, rows=MAX_ROWS;
	int nofnt=FALSE;
	int x, y, y0, y1;
	int i, ret, fd;
	
	browser_cd=TRUE;
	browser_up=FALSE;
	browser_pushed=TRUE;
	browser_sel=0;
	browser_nfiles=0;

	if(cnfmode==TRUE) strcpy(ext, "elf");
	else if(cnfmode==USBD_IRX_CNF) strcpy(ext, "irx");
	else		strcpy(ext, "*");

	if(cnfmode!=USBD_IRX_CNF) strcpy(path, LastDir);
	mountedParty[0][0]=0;
	mountedParty[1][0]=0;
	clipPath[0] = 0;
	nclipFiles = 0;
	browser_cut = 0;
	title_show=FALSE;
	while(1){
		waitPadReady(0, 0);
		if(readpad()){
			if(new_pad) browser_pushed=TRUE;
			if(new_pad & PAD_UP)
				browser_sel--;
			else if(new_pad & PAD_DOWN)
				browser_sel++;
			else if(new_pad & PAD_LEFT)
				browser_sel-=rows/2;
			else if(new_pad & PAD_RIGHT)
				browser_sel+=rows/2;
			else if(new_pad & PAD_TRIANGLE)
				browser_up=TRUE;
			else if((swapKeys && new_pad & PAD_CROSS)
			     || (!swapKeys && new_pad & PAD_CIRCLE) ){
				if(files[browser_sel].stats.attrFile & MC_ATTR_SUBDIR){
					if(!strcmp(files[browser_sel].name,".."))
						browser_up=TRUE;
					else {
						strcat(path, files[browser_sel].name);
						strcat(path, "/");
						browser_cd=TRUE;
					}
				}else{
					sprintf(out, "%s%s", path, files[browser_sel].name);
					// Must to include a function for check IRX Header 
					if((cnfmode!=USBD_IRX_CNF) && (checkELFheader(out)<0)){
						browser_pushed=FALSE;
						sprintf(msg0, "This file isn't ELF.");
						out[0] = 0;
					}else{
						strcpy(LastDir, path);
						break;
					}
				}
			}
			if(cnfmode){
				if(new_pad & PAD_SQUARE) {
					if(cnfmode!=USBD_IRX_CNF){
						if(!strcmp(ext,"*")) strcpy(ext, "elf");
						else				 strcpy(ext, "*");
					}else{
						if(!strcmp(ext,"*")) strcpy(ext, "irx");
						else				 strcpy(ext, "*");
					}
					browser_cd=TRUE;
				}else if((!swapKeys && new_pad & PAD_CROSS)
				      || (swapKeys && new_pad & PAD_CIRCLE) ){
					if(mountedParty[0][0]!=0) fileXioUmount("pfs0:");
					return;
				}
			}else{
				if(new_pad & PAD_R1) {
					ret = menu(path, files[browser_sel].name);
					if(ret==COPY || ret==CUT){
						strcpy(clipPath, path);
						if(nmarks>0){
							for(i=nclipFiles=0; i<browser_nfiles; i++)
								if(marks[i])
									clipFiles[nclipFiles++]=files[i];
						}else{
							clipFiles[0]=files[browser_sel];
							nclipFiles = 1;
						}
						sprintf(msg0, "Copied to the Clipboard");
						browser_pushed=FALSE;
						if(ret==CUT)	browser_cut=TRUE;
						else			browser_cut=FALSE;
					} else if(ret==DELETE){
						if(nmarks==0){
							if(title_show && files[browser_sel].title[0])
								sprintf(tmp,"%s",files[browser_sel].title);
							else{
								sprintf(tmp,"%s",files[browser_sel].name);
								if(files[browser_sel].stats.attrFile & MC_ATTR_SUBDIR)
									strcat(tmp,"/");
							}
							strcat(tmp, "\nDelete?");
							ret = ynDialog(tmp);
						}else
							ret = ynDialog("Mark Files Delete?");
						
						if(ret>0){
							if(nmarks==0){
								strcpy(tmp, files[browser_sel].name);
								if(files[browser_sel].stats.attrFile & MC_ATTR_SUBDIR) strcat(tmp,"/");
								strcat(tmp, " deleting");
								drawMsg(tmp);
								ret=delete(path, &files[browser_sel]);
							}else{
								for(i=0; i<browser_nfiles; i++){
									if(marks[i]){
										strcpy(tmp, files[i].name);
										if(files[i].stats.attrFile & MC_ATTR_SUBDIR) strcat(tmp,"/");
										strcat(tmp, " deleting");
										drawMsg(tmp);
										ret=delete(path, &files[i]);
										if(ret<0) break;
									}
								}
							}
							if(ret>=0) browser_cd=TRUE;
							else{
								strcpy(msg0, "Delete Failed");
								browser_pushed = FALSE;
							}
						}
					} else if(ret==RENAME){
						strcpy(tmp, files[browser_sel].name);
						if(keyboard(tmp, 36)>=0){
							if(Rename(path, &files[browser_sel], tmp)<0){
								browser_pushed=FALSE;
								strcpy(msg0, "Rename Failed");
							}else
								browser_cd=TRUE;
						}
					} else if(ret==PASTE)	submenu_func_Paste(msg0, path);
					else if(ret==MCPASTE)	submenu_func_mcPaste(msg0, path);
					else if(ret==NEWDIR){
						tmp[0]=0;
						if(keyboard(tmp, 36)>=0){
							ret = newdir(path, tmp);
							if(ret == -17){
								strcpy(msg0, "directory already exists");
								browser_pushed=FALSE;
							}else if(ret < 0){
								strcpy(msg0, "NewDir Failed");
								browser_pushed=FALSE;
							}else{
								strcat(path, tmp);
								strcat(path, "/");
								browser_cd=TRUE;
							}
						}
					} else if(ret==GETSIZE) submenu_func_GetSize(msg0, path, files);
				}else if((!swapKeys && new_pad & PAD_CROSS)
				      || (swapKeys && new_pad & PAD_CIRCLE) ){
					if(browser_sel!=0 && path[0]!=0 && strcmp(path,"hdd0:/")){
						if(marks[browser_sel]){
							marks[browser_sel]=FALSE;
							nmarks--;
						}else{
							marks[browser_sel]=TRUE;
							nmarks++;
						}
					}
					browser_sel++;
				} else if(new_pad & PAD_SQUARE) {
					if(path[0]!=0 && strcmp(path,"hdd0:/")){
						for(i=1; i<browser_nfiles; i++){
							if(marks[i]){
								marks[i]=FALSE;
								nmarks--;
							}else{
								marks[i]=TRUE;
								nmarks++;
							}
						}
					}
				} else if((new_pad & PAD_L1) || (new_pad & PAD_L2)) {
					title_show = !title_show;
					title_sort = !!(new_pad & PAD_L1);
					if(elisaFnt==NULL && nofnt==FALSE){
						sprintf(tmp, "%s%s", LaunchElfDir, "ELISA100.FNT");
						if(!strncmp(tmp, "cdrom", 5)) strcat(tmp, ";1");
						fd = fioOpen(tmp, O_RDONLY);
						if(fd>0){
							ret = fioLseek(fd,0,SEEK_END);
							if(ret==55016){
								elisaFnt = (char*)malloc(ret);
								fioLseek(fd,0,SEEK_SET);
								fioRead(fd, elisaFnt, ret);
								fioClose(fd);
							}
						}else
							nofnt = TRUE;
					}
					if(elisaFnt!=NULL){
						if(title_show) rows=19;
						else	  rows=MAX_ROWS;
					}
					browser_cd=TRUE;
				} else if(new_pad & PAD_SELECT){
					if(mountedParty[0][0]!=0) fileXioUmount("pfs0:");
					if(mountedParty[1][0]!=0) fileXioUmount("pfs1:");
					return;
				}
			}
		}
		if(browser_up){
			if((p=strrchr(path, '/'))!=NULL)
				*p = 0;
			if((p=strrchr(path, '/'))!=NULL){
				p++;
				strcpy(oldFolder, p);
				*p = 0;
			}else{
				strcpy(oldFolder, path);
				path[0] = 0;
			}
			browser_cd=TRUE;
		}
		if(browser_cd){
			browser_nfiles = setFileList(path, ext, files, cnfmode);
			if(!cnfmode){
				if(!strncmp(path, "mc", 2)){
					mcGetInfo(path[2]-'0', 0, NULL, &mcfreeSpace, NULL);
				}else if(!strncmp(path,"hdd",3)&&strcmp(path,"hdd0:/")){
					freeSpace = fileXioDevctl("pfs0:", PFSCTL_GET_ZONE_FREE, NULL, 0, NULL, 0)
					          * fileXioDevctl("pfs0:", PFSCTL_GET_ZONE_SIZE, NULL, 0, NULL, 0);
					vfreeSpace=TRUE;
				}
			}
			browser_sel=0;
			top=0;
			if(browser_up){
				for(i=0; i<browser_nfiles; i++) {
					if(!strcmp(oldFolder, files[i].name)) {
						browser_sel=i;
						top=browser_sel-3;
						break;
					}
				}
			}
			nmarks = 0;
			memset(marks, 0, MAX_ENTRY);
			browser_cd=FALSE;
			browser_up=FALSE;
		}
		if(strncmp(path,"cdfs",4) && setting->discControl)
			CDVD_Stop();
		if(top > browser_nfiles-rows)	top=browser_nfiles-rows;
		if(top < 0)				top=0;
		if(browser_sel >= browser_nfiles)		browser_sel=browser_nfiles-1;
		if(browser_sel < 0)				browser_sel=0;
		if(browser_sel >= top+rows)		top=browser_sel-rows+1;
		if(browser_sel < top)			top=browser_sel;
		
		clrScr(setting->color[0]);
		x = SCREEN_MARGIN + LINE_THICKNESS + FONT_WIDTH;
		y = SCREEN_MARGIN + FONT_HEIGHT*2 + LINE_THICKNESS + 12;
		if(title_show && elisaFnt!=NULL) y-=2;
		for(i=0; i<rows; i++)
		{
			if(top+i >= browser_nfiles) break;
			if(top+i == browser_sel) color = setting->color[2];
			else			 color = setting->color[3];
			if(files[top+i].stats.attrFile & MC_ATTR_SUBDIR){
				if(!strcmp(files[top+i].name,".."))
					strcpy(tmp,"..");
				else if(title_show && files[top+i].title[0]!=0)
					strcpy(tmp,files[top+i].title);
				else
					sprintf(tmp, "%s/", files[top+i].name);
			}else
				strcpy(tmp,files[top+i].name);
			printXY(tmp, x+4, y/2, color, TRUE);
			if(marks[top+i]) drawChar('*', x-6, y/2, setting->color[3]);
			y += FONT_HEIGHT;
			if(title_show && elisaFnt!=NULL) y+=2;
		}
		if(browser_nfiles > rows)
		{
			itoSprite(setting->color[1],
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-14,
				(SCREEN_MARGIN+FONT_HEIGHT*2+4)/2,
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-12,
				(SCREEN_HEIGHT-SCREEN_MARGIN-FONT_HEIGHT)/2,
				0);
			y0=(SCREEN_HEIGHT-SCREEN_MARGIN*2-FONT_HEIGHT*3-LINE_THICKNESS*2-4)
				* ((double)top/browser_nfiles);
			y1=(SCREEN_HEIGHT-SCREEN_MARGIN*2-FONT_HEIGHT*3-LINE_THICKNESS*2-4)
				* ((double)(top+rows)/browser_nfiles);
			itoSprite(setting->color[1],
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-11,
				(y0+SCREEN_MARGIN+FONT_HEIGHT*2+LINE_THICKNESS+4)/2,
				SCREEN_WIDTH-SCREEN_MARGIN-LINE_THICKNESS-1,
				(y1+SCREEN_MARGIN+FONT_HEIGHT*2+LINE_THICKNESS+4)/2,
				0);
		}
		if(browser_pushed)
			sprintf(msg0, "Path: %s", path);
		if(cnfmode==TRUE){
			if(!strcmp(ext, "*")) {
				if (swapKeys)
					sprintf(msg1, "~:OK ›:Cancel ¢:Up  :*->ELF");
				else
					sprintf(msg1, "›:OK ~:Cancel ¢:Up  :*->ELF");
			} else {
				if (swapKeys)
					sprintf(msg1, "~:OK ›:Cancel ¢:Up  :ELF->*");
				else
					sprintf(msg1, "›:OK ~:Cancel ¢:Up  :ELF->*");
			}
		}else if(cnfmode==USBD_IRX_CNF){
			if(!strcmp(ext, "*")) {
				if (swapKeys)
					sprintf(msg1, "~:OK ›:Cancel ¢:Up  :*->IRX");
				else
					sprintf(msg1, "›:OK ~:Cancel ¢:Up  :*->IRX");
			} else {
				if (swapKeys)
					sprintf(msg1, "~:OK ›:Cancel ¢:Up  :IRX->*");
				else
					sprintf(msg1, "›:OK ~:Cancel ¢:Up  :IRX->*");
			}
 		}else{
			if(title_show) {
				if (swapKeys) 
					sprintf(msg1, "~:OK ¢:Up ›:Mark  :RevMark L1/L2:TitleOFF R1:Menu");
				else
					sprintf(msg1, "›:OK ¢:Up ~:Mark  :RevMark L1/L2:TitleOFF R1:Menu");
			} else {
				if (swapKeys) 
					sprintf(msg1, "~:OK ¢:Up ›:Mark  :RevMark L1/L2:TitleON  R1:Menu");
				else
					sprintf(msg1, "›:OK ¢:Up ~:Mark  :RevMark L1/L2:TitleON  R1:Menu");
			}
		}
		setScrTmp(msg0, msg1);
		if(!strncmp(path, "mc", 2) && !vfreeSpace && !cnfmode){
			if(mcSync(1,NULL,NULL)!=0){
				freeSpace = mcfreeSpace*1024;
				vfreeSpace=TRUE;
			}
		}
		if(vfreeSpace){
			if(freeSpace >= 1024*1024)
				sprintf(tmp, "[%.1fMB free]", (double)freeSpace/1024/1024);
			else if(freeSpace >= 1024)
				sprintf(tmp, "[%.1fKB free]", (double)freeSpace/1024);
			else
				sprintf(tmp, "[%dB free]", freeSpace);
			ret=strlen(tmp);
			itoSprite(setting->color[0],
				SCREEN_WIDTH-SCREEN_MARGIN-(ret+1)*8,
				(SCREEN_MARGIN+FONT_HEIGHT+4)/2,
				SCREEN_WIDTH,
				(SCREEN_MARGIN+FONT_HEIGHT+20)/2, 0);
			printXY(tmp,
				SCREEN_WIDTH-SCREEN_MARGIN-ret*8,
				(SCREEN_MARGIN+FONT_HEIGHT+4)/2,
				setting->color[2], TRUE);
		}
		drawScr();
	}
	
	if(mountedParty[0][0]!=0) fileXioUmount("pfs0:");
	if(mountedParty[1][0]!=0) fileXioUmount("pfs1:");
	return;
}
//--------------------------------------------------------------
void submenu_func_GetSize(char *mess, char *path, FILEINFO *files)
{
	size_t size;
	int	i, text_pos, text_inc, sel=-1;
	char filepath[MAX_PATH];

/*
	int test;
	iox_stat_t stats;
	PS2TIME *time;
*/

	drawMsg("Checking Size...");
	if(nmarks==0){
		size=getFileSize(path, &files[browser_sel]);
		sel = browser_sel; //for stat checking
	}else{
		for(i=size=0; i<browser_nfiles; i++){
			if(marks[i]){
				size+=getFileSize(path, &files[i]);
				sel = i; //for stat checking
			}
			if(size<0) size=-1;
		}
	}
	printf("size result = %d\r\n", size);
	if(size<0){
		strcpy(mess, "Size test Failed");
	}else{
		text_pos = 0;
		if(size >= 1024*1024)
			sprintf(mess, "SIZE = %.1fMB%n", (double)size/1024/1024, &text_inc);
		else if(size >= 1024)
			sprintf(mess, "SIZE = %.1fKB%n", (double)size/1024, &text_inc);
		else
			sprintf(mess, "SIZE = %dB%n", size, &text_inc);
		text_pos += text_inc;
//----- Comment out this section to skip attributes entirely -----
		if((nmarks<2) && (sel>=0)){
			sprintf(filepath, "%s%s", path, files[sel].name);
//----- Start of section for debug display of attributes -----
/*
			printf("path =\"%s\"\r\n", path);
			printf("file =\"%s\"\r\n", files[sel].name);
			if	(!strncmp(filepath, "host:/", 6))
				makeHostPath(filepath+5, filepath+6);
			if(!strncmp(filepath, "hdd", 3))
				test = fileXioGetStat(filepath, &stats);
			else
				test = fioGetstat(filepath, (fio_stat_t *) &stats);
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
			sprintf(mess+text_pos, " m=%04X %04d.%02d.%02d %02d:%02d:%02d",
				files[sel].stats.attrFile,
				files[sel].stats._modify.year,
				files[sel].stats._modify.month,
				files[sel].stats._modify.day,
				files[sel].stats._modify.hour,
				files[sel].stats._modify.min,
				files[sel].stats._modify.sec
			);
		}
//----- End of sections that show attributes -----
	}
	browser_pushed = FALSE;
}
//End of submenu_func_GetSize
//--------------------------------------------------------------
void subfunc_Paste(char *mess, char *path)
{
	char tmp[MAX_PATH];
	int i, ret=-1;
	
	drawMsg("Pasting...");
	if(!strcmp(path,clipPath)) goto finished;
	
	for(i=0; i<nclipFiles; i++){
		strcpy(tmp, clipFiles[i].name);
		if(clipFiles[i].stats.attrFile & MC_ATTR_SUBDIR) strcat(tmp,"/");
		strcat(tmp, " pasting");
		drawMsg(tmp);
		PM_flag[0] = PM_NORMAL; //Always use normal mode at top level
		PM_file[0] = -1;        //Thus no attribute file is used here
		ret=copy(path, clipPath, clipFiles[i], 0);
		if(ret < 0) break;
		if(browser_cut){
			ret=delete(clipPath, &clipFiles[i]);
			if(ret<0) break;
		}
	}
	
	if(mountedParty[0][0]!=0){
		fileXioUmount("pfs0:"); mountedParty[0][0]=0;
	}
	
	if(mountedParty[1][0]!=0){
		fileXioUmount("pfs1:"); mountedParty[1][0]=0;
	}
	
finished:
	if(ret < 0){
		strcpy(mess, "Paste Failed");
		browser_pushed = FALSE;
	}else
		if(browser_cut) nclipFiles=0;
	browser_cd=TRUE;
}
//End of subfunc_Paste
//--------------------------------------------------------------
void submenu_func_Paste(char *mess, char *path)
{
	PasteMode = PM_NORMAL;
	subfunc_Paste(mess, path);
}
//End of submenu_func_Paste
//--------------------------------------------------------------
void submenu_func_mcPaste(char *mess, char *path)
{
	if(!strncmp(path, "mc", 2)){
		PasteMode = PM_MC_RESTORE;
	} else {
		PasteMode = PM_MC_BACKUP;
	}
	subfunc_Paste(mess, path);
}
//End of submenu_func_Paste
//--------------------------------------------------------------
//End of file: filer.c
//--------------------------------------------------------------
