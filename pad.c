//---------------------------------------------------------------------------
// File name:   pad.c
//---------------------------------------------------------------------------
#include "launchelf.h"

static char padBuf_t[2][256] __attribute__((aligned(64)));
struct padButtonStatus buttons_t[2];
u32 paddata, paddata_t[2];
u32 old_pad = 0, old_pad_t[2] = {0, 0};
u32 new_pad, new_pad_t[2];

//---------------------------------------------------------------------------
// read PAD
int readpad(void)
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
				new_pad_t[port] = paddata_t[port] & ~old_pad_t[port];
				if(old_pad_t[port]==paddata_t[port]){
					n[port]++;
					if(ITO_VMODE_AUTO==ITO_VMODE_NTSC){
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
// Wait for any PAD, but only accept connected states
void waitAnyPadReady(void)
{
	int state_1, state_2;

	state_1 = padGetState(0, 0);
	state_2 = padGetState(1, 0);
	while((state_1 != PAD_STATE_STABLE) && (state_2 != PAD_STATE_STABLE)
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
