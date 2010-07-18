/* execute.h -- Header file for architecture dependent execute.c

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
   Copyright (C) 2008 Embecosm Limited
   Copyright (C) 2009 Stefan Wallentowitz, stefan.wallentowitz@tum.de

   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

   This file is part of Or1ksim, the OpenRISC 1000 Architectural Simulator.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* This program is commented throughout in a fashion suitable for processing
   with Doxygen. */


#ifndef EXECUTE__H
#define EXECUTE__H

#include "spr-defs.h"
#include "opcode/or32.h"
#include "abstract.h"

#if DYNAMIC_EXECUTION
#include "setjmp.h"
#include "dyn-rec.h"
#endif


#define CURINSN(INSN) (strcmp(cur->insn, (INSN)) == 0)

/*! Sets a new SPR_SR_OV value, based on next register value */
#if SET_OV_FLAG
#define SET_OV_FLAG_FN(value) \
  if((value) & 0x80000000) \
    cpu_state.sprs[SPR_SR] |= SPR_SR_OV; \
  else \
    cpu_state.sprs[SPR_SR] &= ~SPR_SR_OV
#else
#define SET_OV_FLAG_FN(value)
#endif

/*! History of execution */
struct hist_exec
{
  oraddr_t addr;
  struct hist_exec *prev;
  struct hist_exec *next;
};

/* Prototypes for external use */
extern void      dumpreg (or1ksim *sim);
extern void      dump_exe_log (or1ksim *sim);
extern int       cpu_clock (or1ksim *sim);
extern void      cpu_reset (or1ksim *sim);
extern uorreg_t  evalsim_reg (or1ksim *sim, unsigned int  regno);
extern void      setsim_reg (or1ksim *sim, unsigned int  regno,
			     uorreg_t      value);
extern uorreg_t  eval_operand_val (uint32_t               insn,
				   struct insn_op_struct *opd);
extern void      analysis (or1ksim *sim, struct iqueue_entry *current);
extern void      exec_main (or1ksim *sim);
extern int       depend_operands (struct iqueue_entry *prev,
				  struct iqueue_entry *next);
#endif  /* EXECUTE__H */
