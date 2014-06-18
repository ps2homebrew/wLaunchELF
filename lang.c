//---------------------------------------------------------------------------
//File name:    lang.c
//---------------------------------------------------------------------------
#include "launchelf.h"

#define lang(id, name, value) u8 Str_##name [] = value;
#include "lang.h"
#undef lang

Language Lang_Default[]={
#define lang(id, name, value) {Str_##name},
#include "lang.h"
#undef lang
	{NULL}
};

Language Lang_String[sizeof(Lang_Default) / sizeof(Lang_Default[0])];
Language Lang_Extern[sizeof(Lang_Default) / sizeof(Lang_Default[0])];

Language *External_Lang_Buffer = NULL;
int lang_initialized_f = 0;

//---------------------------------------------------------------------------
// get_LANG_string is the main parser called for each language dependent
// string in a language header file. (eg: "Francais.h" or "Francais.lng")
// Call values for all input arguments should be addresses of string pointers
// LANG_p_p is for file to be scanned, moved to point beyond scanned data.
// id_p_p is for a string defining the index value (suitable for 'atoi')
// value_p_p is for the string value itself (not NUL-terminated)
// The function returns the length of each string found, but -1 at EOF,
// and various error codes less than -1 (-2 etc) for various syntax errors,
// which also applies to EOF occurring where valid macro parts are expected.
//---------------------------------------------------------------------------
int	get_LANG_string(u8 **LANG_p_p, u8 **id_p_p, u8 **value_p_p)
{
	u8 *cp, *ip, *vp, *tp = *LANG_p_p;
	int ret, length;

	ip = NULL;
	vp = NULL;
	ret = -1;

start_line:
	while(*tp<=' ' && *tp>'\0') tp+=1;     //Skip leading whitespace, if any
	if(*tp=='\0')	goto exit;               //but exit at EOF
	//Current pos is potential "lang(" entry, but we must verify this
	if(tp[0]=='/' && tp[1]=='/')           //It may be a comment line
	{                                      //We must skip a comment line
		while(*tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1;  //Seek line end
		goto start_line;                     //Go back to try next line
	}
	ret = -2;
	//Here tp points to a non-zero string that is not a comment
	if(strncmp(tp, "lang", 4)) goto exit;  //Return error if not 'lang' macro
	tp+=4;                                 //but if it is, step past that name
	ret = -3;
	while(*tp<=' ' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //skip inline whitespace
	if(*tp=='\0')	goto exit;               //but exit at EOF
	ret = -4;
	//Here tp points to a non-zero string that should be an opening parenthesis
	if(*tp!='(') goto exit;                //Return error if no opening parenthesis
	tp+=1;                                 //but if it is, step past this character
	ret = -5;
	while(*tp<=' ' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //skip inline whitespace
	if(*tp=='\0')	goto exit;               //but exit at EOF
	ret = -6;
	//Here tp points to a non-zero string that should be an index number
	if(*tp<'0' || *tp>'9') goto exit;      //Return error if it's not a number
	ip = tp;                               //but if it is, save this pos as id start
	while(*tp>='0' && *tp<='9') tp+=1;     //skip past the index number
	ret = -7;
	while(*tp<=' ' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //skip inline whitespace
	if(*tp=='\0')	goto exit;               //but exit at EOF
	ret = -8;
	//Here tp points to a non-zero string that should be a comma
	if(*tp!=',') goto exit;                //Return error if no comma after index
	tp+=1;                                 //but if present, step past that comma
	ret = -9;
	while(*tp<=' ' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //skip inline whitespace
	if(*tp=='\0')	goto exit;               //but exit at EOF
	ret = -10;
	//Here tp points to a non-zero string that should be a symbolic string name
	//But we don't need to process this for language switch purposes, so we ignore it
	//This may be changed later, to use the name for generating error messages
	while(*tp!=',' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //seek inline comma
	if(*tp!=',') goto exit;                //Return error if no comma after string name
	tp+=1;                                 //but if present, step past that comma
	ret = -11;
	while(*tp<=' ' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //skip inline whitespace
	if(*tp=='\0')	goto exit;               //but exit at EOF
	ret = -12;
	//Here tp points to a non-zero string that should be the opening quote character
	if(*tp!='\"') goto exit;               //Return error if no opening quote
	tp+=1;                                 //but if present, step past that quote
	ret = -13;
	vp = tp;                               //save this pos as value start
close_quote:
	while(*tp!='\"' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //seek inline quote
	if(*tp!='\"') return -13;              //Return error if no closing quote
  cp = tp-1;                             //save previous pos as check pointer
  tp+=1;                                 //step past the quote character
  if(*cp=='\\') goto close_quote;        //if this was an 'escaped' quote, try again
  //Here tp points to the character after the closing quote.
  length = (tp-1) - vp;                  //prepare string length for return value
	ret = -14;
	while(*tp<=' ' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //skip inline whitespace
	if(*tp=='\0')	goto exit;               //but exit at EOF
	ret = -15;
	//Here tp points to a non-zero string that should be closing parenthesis
	if(*tp!=')') goto exit;                //Return error if no closing parenthesis
	tp+=1;                                 //but if present, step past the parenthesis
	ret = -16;
	while(*tp<=' ' && *tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1; //skip inline whitespace
	if(*tp=='\0')	goto exit;              //but exit at EOF
	//Here tp points to a non-zero string that should be line end or a comment
	if(tp[0]!='/' || tp[1]!='/') goto finish_line; //if no comment, go handle line end
	ret = -17;
	while(*tp!='\r' && *tp!='\n' && *tp>'\0') tp+=1;  //Seek line end
	if(*tp=='\0')	goto exit;              //but exit at EOF
finish_line:
	ret = -18;
	if(*tp!='\r' && *tp!='\n') goto exit;  //Return error if not valid line end
	if(tp[0]=='\r' && tp[1]=='\n') tp+=1;  //Step an extra pos for CR+LF
	tp+=1;                                 //Step past valid line end
  //Here tp points beyond the line of the processed string, so we're done
  ret = length;

exit:
	*LANG_p_p = tp;                        //return new LANG file position
	*id_p_p = ip;                          //return found index
	*value_p_p = vp;                       //return found string value
	return ret;                           //return control to caller
}
//Ends get_LANG_string
//---------------------------------------------------------------------------
void Load_External_Language(void)
{
	int error_id = -1;
	int test = 0;

	if(External_Lang_Buffer!=NULL){  //if an external buffer was allocated before
		free(External_Lang_Buffer);    //release that buffer before the new attempt
		External_Lang_Buffer = NULL;
	}

	Language* Lang = Lang_Default;
	memcpy(Lang_String, Lang, sizeof(Lang_String));

	if(strlen(setting->lang_file)!=0){ //if language file string set
		char filePath[MAX_PATH];

		genFixPath(setting->lang_file, filePath);
		int fd = genOpen(filePath, O_RDONLY);
		if(fd >= 0){                         //if file opened OK
    	int file_size = genLseek(fd, 0, SEEK_END);

			error_id = -2;
    	if (file_size > 0) {               //if file size OK
				u32 index = 0;
				u8 *file_bp, *file_tp, *lang_bp, *lang_tp, *oldf_tp;;
				u8 *id_p, *value_p;
				int lang_size = 0;

				error_id = -3;
 	 			file_bp = (u8*) malloc(file_size + 1);
 	 			if(file_bp == NULL)
 	 				goto aborted_1;

				error_id = -4;
				genLseek(fd, 0, SEEK_SET);
				if(genRead(fd, file_bp, file_size) != file_size)
					goto release_1;
				file_bp[file_size] = '\0'; //enforce termination at buffer end

				error_id = -5;
				file_tp = file_bp;
				while(1){
					oldf_tp = file_tp;
					test = get_LANG_string(&file_tp, &id_p, &value_p);
					if(test==-1)                   //if EOF reached without other error
						break;                       //break from the loop normally
					if(test<0 || atoi(id_p)>=LANG_COUNT)  //At any fatal error result
						goto release_1;                     //go release file buffer
					lang_size += test + 1;         //Include terminator space for total size
				}
				//Here lang_size is the space needed for real language buffer,

				error_id = -6;
 	 			lang_bp = (u8*) malloc(lang_size + 1); //allocate real language buffer
 	 			if(lang_bp == NULL)
 	 				goto release_1;

 	 			file_tp = file_bp;
 	 			lang_tp = lang_bp;
				while((test = get_LANG_string(&file_tp, &id_p, &value_p)) >= 0){
					index = atoi(id_p);                  //get the string index
					Lang_Extern[index].String = lang_tp; //save pointer to this string base
					strncpy(lang_tp, value_p, test);     //transfer the string
					lang_tp[test] = '\0';                //transfer a terminator
					lang_tp += test + 1;                 //move dest pointer past this string
				}
				External_Lang_Buffer = (Language*) lang_bp;  //Save base pointer for releases
				Lang = Lang_Extern;
				error_id = 0;
release_1:
				free(file_bp);
			}  // end if clause for file size OK
aborted_1:
			genClose( fd );
		}  // end if clause for file opened OK
	} // end if language file string set

	memcpy(Lang_String, Lang, sizeof(Lang_String));

	int i;
	char *tmp;

	if(strlen(setting->Misc)>0){
		for(i=0; i<15; i++){	//Loop to rename the ELF paths with new language for launch keys
			if((i<12) || (setting->LK_Flag[i]!=0)){
				if(!strncmp(setting->LK_Path[i], setting->Misc, 5)){
					tmp  = strrchr(setting->LK_Path[i], '/');
					if(!strcmp(tmp+1, setting->Misc_PS2Disc+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2Disc));
					else if(!strcmp(tmp+1, setting->Misc_FileBrowser+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(FileBrowser));
					else if(!strcmp(tmp+1, setting->Misc_PS2Browser+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2Browser));
					else if(!strcmp(tmp+1, setting->Misc_PS2Net+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2Net));
					else if(!strcmp(tmp+1, setting->Misc_PS2PowerOff+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(PS2PowerOff));
					else if(!strcmp(tmp+1, setting->Misc_HddManager+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(HddManager));
					else if(!strcmp(tmp+1, setting->Misc_TextEditor+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(TextEditor));
					else if(!strcmp(tmp+1, setting->Misc_JpgViewer+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(JpgViewer));
					else if(!strcmp(tmp+1, setting->Misc_Configure+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Configure));
					else if(!strcmp(tmp+1, setting->Misc_Load_CNFprev+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Load_CNFprev));
					else if(!strcmp(tmp+1, setting->Misc_Load_CNFnext+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Load_CNFnext));
					else if(!strcmp(tmp+1, setting->Misc_Set_CNF_Path+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Set_CNF_Path));
					else if(!strcmp(tmp+1, setting->Misc_Load_CNF+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(Load_CNF));
					else if(!strcmp(tmp+1, setting->Misc_ShowFont+strlen(setting->Misc)))
						sprintf(setting->LK_Path[i], "%s/%s", LNG(MISC), LNG(ShowFont));
				} // end if Misc
			} // end if LK assigned
		} // end for
	} // end if Misc Initialized

	sprintf(setting->Misc              , "%s/",   LNG(MISC));
	sprintf(setting->Misc_PS2Disc      , "%s/%s", LNG(MISC), LNG(PS2Disc));
	sprintf(setting->Misc_FileBrowser  , "%s/%s", LNG(MISC), LNG(FileBrowser));
	sprintf(setting->Misc_PS2Browser   , "%s/%s", LNG(MISC), LNG(PS2Browser));
	sprintf(setting->Misc_PS2Net       , "%s/%s", LNG(MISC), LNG(PS2Net));
	sprintf(setting->Misc_PS2PowerOff  , "%s/%s", LNG(MISC), LNG(PS2PowerOff));
	sprintf(setting->Misc_HddManager   , "%s/%s", LNG(MISC), LNG(HddManager));
	sprintf(setting->Misc_TextEditor   , "%s/%s", LNG(MISC), LNG(TextEditor));
	sprintf(setting->Misc_JpgViewer    , "%s/%s", LNG(MISC), LNG(JpgViewer));
	sprintf(setting->Misc_Configure    , "%s/%s", LNG(MISC), LNG(Configure));
	sprintf(setting->Misc_Load_CNFprev , "%s/%s", LNG(MISC), LNG(Load_CNFprev));
	sprintf(setting->Misc_Load_CNFnext , "%s/%s", LNG(MISC), LNG(Load_CNFnext));
	sprintf(setting->Misc_Set_CNF_Path , "%s/%s", LNG(MISC), LNG(Set_CNF_Path));
	sprintf(setting->Misc_Load_CNF     , "%s/%s", LNG(MISC), LNG(Load_CNF));
	sprintf(setting->Misc_ShowFont     , "%s/%s", LNG(MISC), LNG(ShowFont));

	lang_initialized_f = 1;
}
//Ends Load_External_Language
//---------------------------------------------------------------------------
//End of file:  lang.c
//---------------------------------------------------------------------------
