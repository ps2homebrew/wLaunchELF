// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//
// ito - free library for PlayStation 2 by Jules - www.mouthshut.net
//
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifndef _ITOSYSCALLS_H
#define _ITOSYSCALLS_H

#ifdef __cplusplus 
	extern "C" {
#endif

#include <itotypes.h>

#define ITO_INTC_GS  			0
#define ITO_INTC_SBUS  			1
#define	ITO_INTC_VSYNC_START	2
#define	ITO_INTC_VSYNC_END		3
#define ITO_INTC_VIF0  			4
#define ITO_INTC_VIF1  			5
#define ITO_INTC_VU0  			6
#define ITO_INTC_VU1  			7
#define ITO_INTC_IPU  			8
#define ITO_INTC_TIM0  			9
#define ITO_INTC_TIM1  			10


#define ITO_DMAC_SIF0			5

#define itoEI itoEnableInterrupts
#define itoDI itoDisableInterrupts

struct t_sema
{
	int32	count;
	int32	max_count;
	int32	init_count;
	int32	wait_threads;
	uint32	attr;
	uint32	option;
};

typedef struct t_sema itoSema;

struct thread_attr
{
	uint32	type;
	void	*func;
	void	*stack;
	uint32	stack_size;
	void	*gp_reg;
	uint32	option;
};

typedef struct thread_attr itoThread;


// FUNCTIONS

void itoiFlushCache(int a);
void itoiPutIMR(uint32 value);

void itoFlushCache(int a);
void itoSetGsCrt(uint8 interlace, uint8 tv_mode, uint8 field);
void itoPutIMR(uint32 value);
void itoLoadPS2Exec(char *name, int n, void *args);
uint8 itoAddIntcHandler(uint8 intc, void* addr, uint16 prev_id);
uint8 itoEnableIntc(uint8 intc);
uint8 itoiEnableIntc(uint8 intc);
void itoSifStopDma();

void itoEnableInterrupts();
void itoDisableInterrupts();

// new
int32 itoAddDmacHandler(uint8 channel, void* addr, uint16 prev_id);

#ifdef __cplusplus 
}
#endif /* __cplusplus */

#endif /* _ITOSYSCALLS_H */
