//---------------------------------------------------------------------------
// File name:   pad.c
//---------------------------------------------------------------------------
#include "launchelf.h"

static char padBuf_t[2][256] __attribute__((aligned(64)));
struct padButtonStatus buttons_t[2];
u32 padtype_t[2];
u32 paddata, paddata_t[2];
u32 old_pad = 0, old_pad_t[2] = {0, 0};
u32 new_pad, new_pad_t[2];
u32 joy_value = 0;
static int test_joy = 0;

//---------------------------------------------------------------------------
// read PAD, without KB, and allow no auto-repeat. This is needed in code
// that is used regardless of VSync cycles, and where KB is not wanted.
//---------------------------------------------------------------------------
int readpad_noKBnoRepeat(void)
{
	int port, state, ret[2];

	for(port=0; port<2; port++){
		if((state=padGetState(port, 0))==PAD_STATE_STABLE
			||(state == PAD_STATE_FINDCTP1)){
			//Deal with cases where pad state is valid for padRead
			ret[port] = padRead(port, 0, &buttons_t[port]);
			if (ret[port] != 0){
				paddata_t[port] = 0xffff ^ buttons_t[port].btns;
				new_pad_t[port] = paddata_t[port] & ~old_pad_t[port];
				old_pad_t[port] = paddata_t[port];
			}
		}else{
			//Deal with cases where pad state is not valid for padRead
			new_pad_t[port]=0;
		}  //ends 'if' testing for state valid for padRead
	}  //ends for
	new_pad = new_pad_t[0]|new_pad_t[1];
	return (ret[0]|ret[1]);
}
//------------------------------
//endfunc readpad_noKBnoRepeat
//---------------------------------------------------------------------------
// read PAD, but ignore KB. This is needed in code with own KB handlers,
// such as the virtual keyboard input routines for 'Rename' and 'New Dir'
//---------------------------------------------------------------------------
int readpad_no_KB(void)
{
	static u64 rpt_time[2]={0,0};
	static int rpt_count[2];
	int port, state, ret[2];

	for(port=0; port<2; port++){
		if((state=padGetState(port, 0))==PAD_STATE_STABLE
			||(state == PAD_STATE_FINDCTP1)){
			//Deal with cases where pad state is valid for padRead
			ret[port] = padRead(port, 0, &buttons_t[port]);
			if (ret[port] != 0){
				paddata_t[port] = 0xffff ^ buttons_t[port].btns;
				if((padtype_t[port] == 2) && (1 & (test_joy++))){//DualShock && time for joy scan
					joy_value=0;
					if(buttons_t[port].rjoy_h >= 0xbf){
						paddata_t[port]=PAD_R3_H1;
						joy_value=buttons_t[port].rjoy_h-0xbf;
					}else if(buttons_t[port].rjoy_h <= 0x40){
						paddata_t[port]=PAD_R3_H0;
						joy_value=-(buttons_t[port].rjoy_h-0x40);
					}else if(buttons_t[port].rjoy_v <= 0x40){
						paddata_t[port]=PAD_R3_V0;
						joy_value=-(buttons_t[port].rjoy_v-0x40);
					}else if(buttons_t[port].rjoy_v >= 0xbf){
						paddata_t[port]=PAD_R3_V1;
						joy_value=buttons_t[port].rjoy_v-0xbf;
					}else if(buttons_t[port].ljoy_h >= 0xbf){
						paddata_t[port]=PAD_L3_H1;
						joy_value=buttons_t[port].ljoy_h-0xbf;
					}else if(buttons_t[port].ljoy_h <= 0x40){
						paddata_t[port]=PAD_L3_H0;
						joy_value=-(buttons_t[port].ljoy_h-0x40);
					}else if(buttons_t[port].ljoy_v <= 0x40){
						paddata_t[port]=PAD_L3_V0;
						joy_value=-(buttons_t[port].ljoy_v-0x40);
					}else if(buttons_t[port].ljoy_v >= 0xbf){
						paddata_t[port]=PAD_L3_V1;
						joy_value=buttons_t[port].ljoy_v-0xbf;
					}
				}
				new_pad_t[port] = paddata_t[port] & ~old_pad_t[port];
				if(old_pad_t[port] == paddata_t[port]){
					//no change of pad data
					if(Timer() > rpt_time[port]){
						new_pad_t[port]=paddata_t[port]; //Accept repeated buttons as new
						rpt_time[port] = Timer() + 40; //Min delay = 40ms => 25Hz repeat
						if(rpt_count[port]++ < 20)
							rpt_time[port] += 43; //Early delays = 83ms => 12Hz repeat
					}
				}else{
					//pad data has changed !
					rpt_count[port] = 0;
					rpt_time[port] = Timer()+400; //Init delay = 400ms
					old_pad_t[port] = paddata_t[port];
				}
			}
		}else{
			//Deal with cases where pad state is not valid for padRead
			new_pad_t[port]=0;
			old_pad_t[port]=0;
		}  //ends 'if' testing for state valid for padRead
	}  //ends for
	new_pad = new_pad_t[0]|new_pad_t[1];
	return (ret[0]|ret[1]);
}
//------------------------------
//endfunc readpad_no_KB
//---------------------------------------------------------------------------
// simPadKB attempts reading data from a USB keyboard, and map this as a
// virtual gamepad. (Very improvised and sloppy, but it should work fine.)
//---------------------------------------------------------------------------
int simPadKB(void)
{
	int	ret, command;
	unsigned char KeyPress;

	if((!setting->usbkbd_used)||(!PS2KbdRead(&KeyPress)))
		return 0;
	if(KeyPress != PS2KBD_ESCAPE_KEY)
		command = KeyPress;
	else {
		PS2KbdRead(&KeyPress);
		command = 0x100+KeyPress;
	}
	ret = 1;  //Assume that the entered key is a valid command
	switch(command) {
		case 0x11B:                 //Escape == Triangle
			new_pad = PAD_TRIANGLE;
			break;
		case 0x00A:                 //Enter == OK
		  if(!swapKeys)
		  	new_pad = PAD_CIRCLE;
		  else
		  	new_pad = PAD_CROSS;
		  break;
		case 0x020:                 //Space == Cancel/Mark
		  if(!swapKeys)
		  	new_pad = PAD_CROSS;
		  else
		  	new_pad = PAD_CIRCLE;
		  break;
		case 0x031:                 //'1' == L1
			new_pad = PAD_L1;
			break;
		case 0x032:                 //'2' == L2
			new_pad = PAD_L2;
			break;
		case 0x033:                 //'3' == L3
			new_pad = PAD_L3;
			break;
		case 0x077:                 //'w' == Up
			new_pad = PAD_UP;
			break;
		case 0x061:                 //'a' == Left
			new_pad = PAD_LEFT;
			break;
		case 0x073:                 //'s' == Right
			new_pad = PAD_RIGHT;
			break;
		case 0x07A:                 //'z' == Down
			new_pad = PAD_DOWN;
			break;
		case 0x030:                 //'0' == R1
			new_pad = PAD_R1;
			break;
		case 0x039:                 //'9' == R2
			new_pad = PAD_R2;
			break;
		case 0x038:                 //'8' == R3
			new_pad = PAD_R3;
			break;
		case 0x069:                 //'i' == Triangle
			new_pad = PAD_TRIANGLE;
			break;
		case 0x06A:                 //'j' == Square
			new_pad = PAD_SQUARE;
			break;
		case 0x06B:                 //'k' == Circle
			new_pad = PAD_CIRCLE;
			break;
		case 0x06D:                 //'m' == Cross
			new_pad = PAD_CROSS;
			break;
		case 0x101:                 //F1 == L1
			new_pad = PAD_L1;
			break;
		case 0x102:                 //F2 == L2
			new_pad = PAD_L2;
			break;
		case 0x103:                 //F3 == L3
			new_pad = PAD_L3;
			break;
		case 0x12C:                 //Up == Up
			new_pad = PAD_UP;
			break;
		case 0x12A:                 //Left == Left
			new_pad = PAD_LEFT;
			break;
		case 0x129:                 //Right == Right
			new_pad = PAD_RIGHT;
			break;
		case 0x12B:                 //Down == Down
			new_pad = PAD_DOWN;
			break;
		case 0x123:                 //Insert == Select
			new_pad = PAD_SELECT;
			break;
		case 0x10C:                 //F12 == R1
			new_pad = PAD_R1;
			break;
		case 0x10B:                 //F11 == R2
			new_pad = PAD_R2;
			break;
		case 0x10A:                 //F10 == R3
			new_pad = PAD_R3;
			break;
		case 0x124:                 //Home == Triangle
			new_pad = PAD_TRIANGLE;
			break;
		case 0x127:                 //End == Square
			new_pad = PAD_SQUARE;
			break;
		case 0x125:                 //PgUp == Circle
			new_pad = PAD_CIRCLE;
			break;
		case 0x128:                 //PgDn == Cross
			new_pad = PAD_CROSS;
			break;
		case 0x126:                 //Delete == Start
			new_pad = PAD_START;
			break;
		default:                    //Unrecognized key => no pad button
			ret = 0;
			break;
	}
	return ret;
}
//------------------------------
//endfunc simPadKB
//---------------------------------------------------------------------------
// readpad will call readpad_no_KB, and if no new pad buttons are found, it
// will also attempt reading data from a USB keyboard, and map this as a
// virtual gamepad. (Very improvised and sloppy, but it should work fine.)
//---------------------------------------------------------------------------
int readpad(void)
{
	int	ret;

	if((ret=readpad_no_KB()) && new_pad)
		return ret;

	return simPadKB();
}
//------------------------------
//endfunc readpad
//---------------------------------------------------------------------------
// readpad_noRepeat calls readpad_noKBnoRepeat, and if no new pad buttons are
// found, it also attempts reading data from a USB keyboard, and map this as
// a virtual gamepad. (Very improvised and sloppy, but it should work fine.)
//---------------------------------------------------------------------------
int readpad_noRepeat(void)
{
	int	ret;

	if((ret=readpad_noKBnoRepeat()) && new_pad)
		return ret;

	return simPadKB();
}
//------------------------------
//endfunc readpad_noRepeat
//---------------------------------------------------------------------------
// Wait for specific PAD, but also accept disconnected state
void waitPadReady(int port, int slot)
{
	int state, lastState;
	char stateString[16];

	state = padGetState(port, slot);
	lastState = -1;
	while((state != PAD_STATE_DISCONN)
		&& (state != PAD_STATE_STABLE)
		&& (state != PAD_STATE_FINDCTP1)){
		if (state != lastState)
			padStateInt2String(state, stateString);
		lastState = state;
		state=padGetState(port, slot);
	}
}
//---------------------------------------------------------------------------
// Wait for any PAD, but also accept disconnected states
void waitAnyPadReady(void)
{
	int state_1, state_2;

	state_1 = padGetState(0, 0);
	state_2 = padGetState(1, 0);
	while((state_1 != PAD_STATE_DISCONN) && (state_2 != PAD_STATE_DISCONN)
		&& (state_1 != PAD_STATE_STABLE) && (state_2 != PAD_STATE_STABLE)
		&& (state_1 != PAD_STATE_FINDCTP1) && (state_2 != PAD_STATE_FINDCTP1)){
		state_1 = padGetState(0, 0);
		state_2 = padGetState(1, 0);
	}
}
//---------------------------------------------------------------------------
// setup PAD
int setupPad(void)
{
	int ret, i, port, state, modes;

	padInit(0);

	for(port=0; port<2; port++){
		padtype_t[port] = 0;  //Assume that we don't have a proper PS2 controller
		if((ret = padPortOpen(port, 0, &padBuf_t[port][0])) == 0)
			return 0;
		waitPadReady(port, 0);
		state = padGetState(port, 0);
		if(state != PAD_STATE_DISCONN){ //if anything connected to this port
			modes = padInfoMode(port, 0, PAD_MODETABLE, -1);
			if (modes != 0){ //modes != 0, so it may be a dualshock type
				for(i=0; i<modes; i++){
					if (padInfoMode(port, 0, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK){
						padtype_t[port] = 2; //flag normal PS2 controller
						break;
					}
				} //ends for (modes)
			} else { //modes == 0, so this is a digital controller
				padtype_t[port] = 1; //flag digital controller
			}
			if(padtype_t[port] == 2)                                        //if DualShock
				padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);   //Set DualShock
			else                                                            //else
				padSetMainMode(port, 0, PAD_MMODE_DIGITAL, PAD_MMODE_UNLOCK);   //Set Digital
			waitPadReady(port, 0);                                          //Await completion
		} else {                                          //Nothing is connected to this port
				padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK); //Fake DualShock
				waitPadReady(port, 0);                                        //Await completion
		}
	} //ends for (port)
	return 1;
}
//---------------------------------------------------------------------------
// End of file: pad.c
//---------------------------------------------------------------------------
