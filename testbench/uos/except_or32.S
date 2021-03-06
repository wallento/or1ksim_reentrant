/* This file is part of test microkernel for OpenRISC 1000. */
/* (C) 2000 Damjan Lampret, lampret@opencores.org */

#include "spr_defs.h"
#include "../board.h"

#define MC_CSR          (0x00)
#define MC_POC          (0x04)
#define MC_BA_MASK      (0x08)
#define MC_CSC(i)       (0x10 + (i) * 8)
#define MC_TMS(i)       (0x14 + (i) * 8)


/*
 * Context is saved to area pointed by pointer in R3. Original
 * R3 is at memory location 0 and task's PC is at memory location 4.
 */
#define SAVEREGS					\
	l.lwz	r3,0(r3);				\
	l.sw	4(r3),r1;				\
	l.sw	8(r3),r2;				\
	l.lwz	r2,0(r0);	/* saving original r3*/ \
	l.sw	12(r3),r2;				\
	l.sw	16(r3),r4;				\
	l.sw	20(r3),r5;				\
	l.sw	24(r3),r6;				\
	l.sw	28(r3),r7;				\
	l.sw	32(r3),r8;				\
	l.sw	36(r3),r9;				\
	l.sw	40(r3),r10;				\
	l.sw	44(r3),r11;				\
	l.sw	48(r3),r12;				\
	l.sw	52(r3),r13;				\
	l.sw	56(r3),r14;				\
	l.sw	60(r3),r15;				\
	l.sw	64(r3),r16;				\
	l.sw	68(r3),r17;				\
	l.sw	72(r3),r18;				\
	l.sw	76(r3),r19;				\
	l.sw	80(r3),r20;				\
	l.sw	84(r3),r21;				\
	l.sw	88(r3),r22;				\
	l.sw	92(r3),r23;				\
	l.sw	96(r3),r24;				\
	l.sw	100(r3),r25;				\
	l.sw	104(r3),r26;				\
	l.sw	108(r3),r27;				\
	l.sw	112(r3),r28;				\
	l.sw	116(r3),r29;				\
	l.sw	120(r3),r30;				\
	l.sw	124(r3),r31;				\
	l.lwz	r2,4(r0);	/* saving original PC*/	\
	l.sw	0(r3),r2;				\
							\
	l.mfspr	r2,r0,SPR_ESR_BASE;				\
	l.sw	128(r3),r2	/* saving SR */

/*
 * Pointer to context is in R3. All registers are loaded and execution is
 * transfered to the loaded context's task
 */
#define LOADREGS_N_GO				\
	l.lwz	r3,0(r3);			\
	l.lwz	r2,0(r3);	/* prepare PC*/	\
	l.mtspr	r0,r2,SPR_EPCR_BASE;		\
						\
	l.lwz	r2,128(r3);	/* prepare SR*/	\
	l.mtspr	r0,r2,SPR_ESR_BASE;			\
						\
	l.lwz	r1,4(r3);			\
	l.lwz	r2,8(r3);			\
	l.lwz	r4,16(r3);			\
	l.lwz	r5,20(r3);			\
	l.lwz	r6,24(r3);			\
	l.lwz	r7,28(r3);			\
	l.lwz	r8,32(r3);			\
	l.lwz	r9,36(r3);			\
	l.lwz	r10,40(r3);			\
	l.lwz	r11,44(r3);			\
	l.lwz	r12,48(r3);			\
	l.lwz	r13,52(r3);			\
	l.lwz	r14,56(r3);			\
	l.lwz	r15,60(r3);			\
	l.lwz	r16,64(r3);			\
	l.lwz	r17,68(r3);			\
	l.lwz	r18,72(r3);			\
	l.lwz	r19,76(r3);			\
	l.lwz	r20,80(r3);			\
	l.lwz	r21,84(r3);			\
	l.lwz	r22,88(r3);			\
	l.lwz	r23,92(r3);			\
	l.lwz	r24,96(r3);			\
	l.lwz	r25,100(r3);			\
	l.lwz	r26,104(r3);			\
	l.lwz	r27,108(r3);			\
	l.lwz	r28,112(r3);			\
	l.lwz	r29,116(r3);			\
	l.lwz	r30,120(r3);			\
	l.lwz	r31,124(r3);			\
						\
	l.lwz	r3,12(r3);	/* prepare r3*/	\
						\
	l.rfe;			/* Call task */	\
	l.nop
	
/*
 * All registers are loaded from save area.
 */
#define LOADREGS				\
	l.lwz	r3,0(r3);			\
	l.lwz	r2,0(r3);	/* prepare PC*/	\
	l.mtspr	r0,r2,SPR_EPCR_BASE;		\
						\
	l.lwz	r2,128(r3);	/* prepare SR*/	\
	l.mtspr	r0,r2,SPR_ESR_BASE;			\
						\
	l.lwz	r1,4(r3);			\
	l.lwz	r2,8(r3);			\
	l.lwz	r4,16(r3);			\
	l.lwz	r5,20(r3);			\
	l.lwz	r6,24(r3);			\
	l.lwz	r7,28(r3);			\
	l.lwz	r8,32(r3);			\
	l.lwz	r9,36(r3);			\
	l.lwz	r10,40(r3);			\
	l.lwz	r11,44(r3);			\
	l.lwz	r12,48(r3);			\
	l.lwz	r13,52(r3);			\
	l.lwz	r14,56(r3);			\
	l.lwz	r15,60(r3);			\
	l.lwz	r16,64(r3);			\
	l.lwz	r17,68(r3);			\
	l.lwz	r18,72(r3);			\
	l.lwz	r19,76(r3);			\
	l.lwz	r20,80(r3);			\
	l.lwz	r21,84(r3);			\
	l.lwz	r22,88(r3);			\
	l.lwz	r23,92(r3);			\
	l.lwz	r24,96(r3);			\
	l.lwz	r25,100(r3);			\
	l.lwz	r26,104(r3);			\
	l.lwz	r27,108(r3);			\
	l.lwz	r28,112(r3);			\
	l.lwz	r29,116(r3);			\
	l.lwz	r30,120(r3);			\
	l.lwz	r31,124(r3);			\
						\
	l.lwz	r3,12(r3);	/* prepare r3*/

/*
 * Set new PC in saved context 
 */
#define SET_CONTEXTPC(AREA,SUBROUTINE,TMPREG)	\
	l.lwz	AREA,0(AREA);			\
	l.movhi	TMPREG,hi(SUBROUTINE);		\
	l.addi	TMPREG,r0,lo(SUBROUTINE);	\
	l.sw	0(AREA),TMPREG;

/*
 * Printf via or1ksim hook 
 */
#if KERNEL_OUTPUT
#define PRINTF(REG,STR)				\
	l.movhi	REG,hi(STR);			\
	l.addi	REG,r0,lo(STR);			\
	l.nop   NOP_PRINTF
#else
#define PRINTF(REG,STR)
#endif

/*
 * Reset Exception handler
 */
.org 0x100
_reset_vector:

  l.movhi r3,hi(MC_BASE_ADDR)
  l.ori   r3,r3,lo(MC_BASE_ADDR)
 
  l.addi  r4,r3,MC_CSC(0)
  l.movhi r5,hi(FLASH_BASE_ADDR)
  l.srai  r5,r5,6
  l.ori   r5,r5,0x0025
  l.sw    0(r4),r5
 
  l.addi  r4,r3,MC_TMS(0)
  l.movhi r5,hi(FLASH_TMS_VAL)
  l.ori   r5,r5,lo(FLASH_TMS_VAL)
  l.sw    0(r4),r5
 
  l.addi  r4,r3,MC_BA_MASK
  l.addi  r5,r0,MC_MASK_VAL
  l.sw    0(r4),r5
 
  l.addi  r4,r3,MC_CSR
  l.movhi r5,hi(MC_CSR_VAL)
  l.ori   r5,r5,lo(MC_CSR_VAL)
  l.sw    0(r4),r5
 
  l.addi  r4,r3,MC_TMS(1)
  l.movhi r5,hi(SDRAM_TMS_VAL)
  l.ori   r5,r5,lo(SDRAM_TMS_VAL)
  l.sw    0(r4),r5
 
  l.addi  r4,r3,MC_CSC(1)
  l.movhi r5,hi(SDRAM_BASE_ADDR)
  l.srai  r5,r5,6
  l.ori   r5,r5,0x0411
  l.sw    0(r4),r5
 
  l.jr    r9
  l.nop

  /* Copy data section */
  l.movhi r3,hi(_src_beg)
  l.ori   r3,r3,lo(_src_beg)
  l.addi  r4,r0,0x200
  l.movhi r5,hi(_except_end)
  l.ori   r5,r5,lo(_except_end)
  l.movhi r6,hi(_except_beg)
  l.ori   r6,r6,lo(_except_beg)
  l.sub   r5,r6,r5
1:
  l.lwz   r6,0(r3)
  l.sw    0(r4),r6
  l.addi  r3,r3,4
  l.addi  r4,r4,4
  l.addi  r5,r5,-4
  l.sfgtsi r5,0
  l.bf    1b
  l.nop

  l.movhi r4,hi(_dst_beg)
  l.ori   r4,r4,lo(_dst_beg)
  l.movhi r5,hi(_dst_end)
  l.ori   r5,r5,lo(_dst_end)
  l.sub   r5,r5,r4
  l.sfeqi r5,0
  l.bf    2f
  l.nop
1:
  l.lwz   r6,0(r3)
  l.sw    0(r4),r6
  l.addi  r3,r3,4
  l.addi  r4,r4,4
  l.addi  r5,r5,-4
  l.sfgtsi r5,0
  l.bf   	1b
  l.nop

2:


  l.movhi r2,hi(_reset)
  l.ori   r2,r2,lo(_reset)
  l.jr    r2
  l.nop

/*
 * Switch to a new context pointed by _task_context
 */
.global _dispatch
.align 4
_dispatch:
	/* load user task GPRs and PC */
	l.movhi	r3,hi(_task_context)
	l.addi	r3,r0,lo(_task_context)
	LOADREGS_N_GO

.section .except, "ax"

/*
 * Bus Error Exception handler
 */
.org 0x0200
_buserr:
	l.nop
	l.sw	0(r0),r3	/* Save r3 */
	PRINTF(r3, _buserr_str)
_hang:
	l.j	_hang
	l.nop

_buserr_str:
        .ascii  "Bus error exception.\n\000"

/*
 * External Interrupt Exception handler
 */
.org 0x800
_extint:
	l.nop
	l.sw	0(r0),r3	/* Save r3 */
	PRINTF(r3,_extint_str)
	l.mfspr	r3,r0,SPR_EPCR_BASE	/* Get EPCR */
	l.sw	4(r0),r3	/* and save it */

	/* now save user task context */
	l.movhi	r3,hi(_task_context)
	l.addi	r3,r0,lo(_task_context)
	SAVEREGS

	/* set kernel context's PC to kernel's scheduler */
	l.movhi	r3,hi(_kernel_context)
	l.addi	r3,r0,lo(_kernel_context)
	SET_CONTEXTPC(r3,_int_main,r4)

	/* load kernel context */
	l.movhi	r3,hi(_kernel_context)
	l.addi	r3,r0,lo(_kernel_context)
	LOADREGS
	
	l.movhi	r3,hi(_int_main)
	l.addi	r3,r0,lo(_int_main)
	l.jr    r3
	l.nop

_extint_str:
        .ascii  "External interrupt exception.\n\000"

/*
 * System Call Exception handler
 */
.org 0x0c00
_syscall:
	l.nop
	l.sw	0(r0),r3	/* Save r3 */
	PRINTF(r3,_syscall_str)
	l.mfspr	r3,r0,SPR_EPCR_BASE	/* Get EPCR */
	l.addi	r3,r3,4		/* increment because EPCR instruction was already executed */
	l.sw	4(r0),r3	/* and save it */

	/* now save user task context */
	l.movhi	r3,hi(_task_context)
	l.addi	r3,r0,lo(_task_context)
	SAVEREGS

	/* set kernel context's PC to kernel's syscall entry */
	l.movhi	r3,hi(_kernel_context)
	l.addi	r3,r0,lo(_kernel_context)
	SET_CONTEXTPC(r3,_kernel_syscall,r4)

	/* load kernel context */
	l.movhi	r3,hi(_kernel_context)
	l.addi	r3,r0,lo(_kernel_context)
	LOADREGS_N_GO

_syscall_str:
        .ascii  "System call exception.\n\000"


