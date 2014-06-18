//---------------------------------------------------------------------------
// File name:   pad.c
//---------------------------------------------------------------------------
#include "launchelf.h"

static char padBuf_t[2][256] __attribute__((aligned(64)));
struct padButtonStatus buttons_t[2];
u32 paddata, paddata_t[2];
u32 old_pad = 0, old_pad_t[2] = {0, 0};
u32 new_pad, new_pad_t[2];
static int test_joy;

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
	static int n[2]={0,0}, nn[2]={0,0};
	int port, state, ret[2];

	for(port=0; port<2; port++){
		if((state=padGetState(port, 0))==PAD_STATE_STABLE
			||(state == PAD_STATE_FINDCTP1)){
			//Deal with cases where pad state is valid for padRead
			ret[port] = padRead(port, 0, &buttons_t[port]);
			if (ret[port] != 0){
				paddata_t[port] = 0xffff ^ buttons_t[port].btns;
				if(++test_joy==2) test_joy=0;
				if(test_joy){
					if(buttons_t[port].rjoy_h >= 0xf0)
						paddata_t[port]=PAD_R3_H1;
					else if(buttons_t[port].rjoy_h <= 0x0f)
						paddata_t[port]=PAD_R3_H0;
					else if(buttons_t[port].rjoy_v <= 0x0f)
						paddata_t[port]=PAD_R3_V0;
					else if(buttons_t[port].rjoy_v >= 0xf0)
						paddata_t[port]=PAD_R3_V1;
					else if(buttons_t[port].ljoy_h >= 0xf0)
						paddata_t[port]=PAD_L3_H1;
					else if(buttons_t[port].ljoy_h <= 0x0f)
						paddata_t[port]=PAD_L3_H0;
					else if(buttons_t[port].ljoy_v <= 0x0f)
						paddata_t[port]=PAD_L3_V0;
					else if(buttons_t[port].ljoy_v >= 0xf0)
						paddata_t[port]=PAD_L3_V1;
				}
				new_pad_t[port] = paddata_t[port] & ~old_pad_t[port];
				if(old_pad_t[port]==paddata_t[port]){
					n[port]++;
					if(gsKit_detect_signal()==GS_MODE_NTSC){
						if(n[port]>=25){
							new_pad_t[port]=paddata_t[port];
							if(nn[port]++ < 20)	n[port]=20;
							else			n[port]=23;
						}
					}else{
						if(n[port]>=21){
							new_pad_t[port]=paddata_t[port];
							if(nn[port]++ < 20)	n[port]=17;
							else			n[port]=19;
						}
					}
				}else{
					n[port]=0;
					nn[port]=0;
					old_pad_t[port] = paddata_t[port];
				}
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

	if((ret=readpad_no_KB()) && new_pad)
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
		if((ret = padPortOpen(port, 0, &padBuf_t[port][0])) == 0)
			return 0;
		waitPadReady(port, 0);
		state = padGetState(port, 0);
		if(state != PAD_STATE_DISCONN){
			modes = padInfoMode(port, 0, PAD_MODETABLE, -1);
			if (modes != 0){
				i = 0;
				do{
					if (padInfoMode(port, 0, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK){
						padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
						break;
					}
					i++;
				} while (i < modes);
			}
		}
	}
	return 1;
}
//---------------------------------------------------------------------------
// End of file: pad.c
//---------------------------------------------------------------------------
