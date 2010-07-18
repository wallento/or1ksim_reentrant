/* execute.c -- OR1K architecture dependent simulation

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
   Copyright (C) 2005 György `nog' Jeney, nog@sdf.lonestar.org
   Copyright (C) 2008 Embecosm Limited
   Copyright (C) 2009 Stefan Wallentowitz, stefan.wallentowitz@tum.de
  
   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>
  
   This file is part of OpenRISC 1000 Architectural Simulator.
  
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your option)
   any later version.
  
   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.
  
   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* This program is commented throughout in a fashion suitable for processing
   with Doxygen. */


/* Most of the OR1K simulation is done in here.

   When SIMPLE_EXECUTION is defined below a file insnset.c is included!
*/

/* Autoconf and/or portability configuration */
#include "config.h"
#include "port.h"

/* System includes */
#include <stdlib.h>

/* Package includes */
#include "execute.h"
#include "toplevel-support.h"
#include "except.h"
#include "labels.h"
#include "gdbcomm.h"
#include "sched.h"
#include "stats.h"
#include "opcode/or32.h"
#include "dmmu.h"
#include "immu.h"
#include "sim-cmd.h"
#include "vapi.h"
#include "debug-unit.h"
#include "branch-predict.h"
#include "support/simprintf.h"
#include "sprs.h"
#include "rsp-server.h"


/* Includes and macros for simple execution */
#if SIMPLE_EXECUTION

#define SET_PARAM0(val) set_operand(0, val, current->insn_index, current->insn)

#define PARAM0 eval_operand(0, current->insn_index, current->insn)
#define PARAM1 eval_operand(1, current->insn_index, current->insn)
#define PARAM2 eval_operand(2, current->insn_index, current->insn)

#define INSTRUCTION(name) void name (struct iqueue_entry *current)

#endif	/* SIMPLE_EXECUTION */

/* Forward declaration of static functions */
#if !(DYNAMIC_EXECUTION)
static void decode_execute (or1ksim *sim,struct iqueue_entry *current);
#endif

/*---------------------------------------------------------------------------*/
/*!Get an actual value of a specific register

   Implementation specific. Abort if we are given a duff register. Only used
   externally to support simprintf().

   @param[in] regno  The register of interest

   @return  The value of the register                                        */
/*---------------------------------------------------------------------------*/
uorreg_t
evalsim_reg (or1ksim *sim, unsigned int  regno)
{
  if (regno < MAX_GPRS)
    {
#if RAW_RANGE_STATS
      int delta = (sim->runtime.sim.cycles - raw_stats.reg[regno]);

      if ((unsigned long) delta < (unsigned long) RAW_RANGE)
	{
	  raw_stats.range[delta]++;
	}
#endif /* RAW_RANGE */

      return sim->cpu_state.reg[regno];
    }
  else
    {
      PRINTF ("\nABORT: read out of registers\n");
      sim_done ( sim );
      return 0;
    }
}	/* evalsim_reg() */


/*---------------------------------------------------------------------------*/
/*!Set a specific register with value

   Implementation specific. Abort if we are given a duff register.

   @param[in] regno  The register of interest
   @param[in] value  The value to be set                                     */
/*---------------------------------------------------------------------------*/
void
setsim_reg (or1ksim *sim, unsigned int  regno,
	    uorreg_t      value)
{
  if (regno == 0)		/* gpr0 is always zero */
    {
      value = 0;
    }

  if (regno < MAX_GPRS)
    {
      sim->cpu_state.reg[regno] = value;
    }
  else
    {
      PRINTF ("\nABORT: write out of registers\n");
      sim_done ( sim );
    }

#if RAW_RANGE_STATS
  raw_stats.reg[regno] = sim->runtime.sim.cycles;
#endif /* RAW_RANGE */

}	/* setsim_reg() */


/*---------------------------------------------------------------------------*/
/*!Evaluates source operand operand

   Implementation specific. Declared global, although this is only actually
   required for DYNAMIC_EXECUTION,

   @param[in] insn  The instruction
   @param[in] opd   The operand

   @return  The value of the source operand                                  */
/*---------------------------------------------------------------------------*/
uorreg_t
eval_operand_val (uint32_t               insn,
		  struct insn_op_struct *opd)
{
  unsigned long  operand = 0;
  unsigned long  sbit;
  unsigned int   nbits = 0;

  while (1)
    {
      operand |=
	((insn >> (opd->type & OPTYPE_SHR)) & ((1 << opd->data) - 1)) <<
	nbits;
      nbits += opd->data;

      if (opd->type & OPTYPE_OP)
	{
	  break;
	}

      opd++;
    }

  if (opd->type & OPTYPE_SIG)
    {
      sbit = (opd->type & OPTYPE_SBIT) >> OPTYPE_SBIT_SHR;

      if (operand & (1 << sbit))
	{
	  operand |= ~REG_C (0) << sbit;
	}
    }

  return operand;

}	/* eval_operand_val() */


/*---------------------------------------------------------------------------*/
/*!Does source operand depend on computation of dest operand?

      Cycle t                 Cycle t+1
  dst: irrelevant         src: immediate                  always 0
  dst: reg1 direct        src: reg2 direct                0 if reg1 != reg2
  dst: reg1 disp          src: reg2 direct                always 0
  dst: reg1 direct        src: reg2 disp                  0 if reg1 != reg2
  dst: reg1 disp          src: reg2 disp                  always 1 (store must
                                                          finish before load)
  dst: flag               src: flag                       always 1

  @param[in] prev  Previous instruction
  @param[in] next  Next instruction

  @return  Non-zero if yes.                                                  */
/*---------------------------------------------------------------------------*/
static int
check_depend (or1ksim *sim, struct iqueue_entry *prev,
	      struct iqueue_entry *next)
{
  /* Find destination type. */
  unsigned long          type = 0;
  int                    prev_dis;
  int                    next_dis;
  orreg_t                prev_reg_val = 0;
  struct insn_op_struct *opd;

  if (or32_opcodes[prev->insn_index].flags & OR32_W_FLAG
      && or32_opcodes[next->insn_index].flags & OR32_R_FLAG)
    {
      return  1;
    }

  opd      = sim->op_start[prev->insn_index];
  prev_dis = 0;

  while (1)
    {
      if (opd->type & OPTYPE_DIS)
	{
	  prev_dis = 1;
	}

      if (opd->type & OPTYPE_DST)
	{
	  type = opd->type;

	  if (prev_dis)
	    {
	      type |= OPTYPE_DIS;
	    }

	  /* Destination is always a register */
	  prev_reg_val = eval_operand_val (prev->insn, opd);
	  break;
	}

      if (opd->type & OPTYPE_LAST)
	{
	  return 0;		/* Doesn't have a destination operand */
	}

      if (opd->type & OPTYPE_OP)
	{
	  prev_dis = 0;
	}

      opd++;
    }

  /* We search all source operands - if we find confict => return 1 */
  opd      = sim->op_start[next->insn_index];
  next_dis = 0;

  while (1)
    {
      if (opd->type & OPTYPE_DIS)
	{
	  next_dis = 1;
	}

      /* This instruction sequence also depends on order of execution:
           l.lw r1, k(r1)
           l.sw k(r1), r4
         Here r1 is a destination in l.sw */

      /* FIXME: This situation is not handeld here when r1 == r2:
           l.sw k(r1), r4
           l.lw r3, k(r2) */
      if (!(opd->type & OPTYPE_DST) || (next_dis && (opd->type & OPTYPE_DST)))
	{
	  if (opd->type & OPTYPE_REG)
	    {
	      if (eval_operand_val (next->insn, opd) == prev_reg_val)
		{
		  return 1;
		}
	    }
	}

      if (opd->type & OPTYPE_LAST)
	{
	  break;
	}

      opd++;
    }

  return  0;

}	/* check_depend() */


/*---------------------------------------------------------------------------*/
/*!Should instruction NOT be executed?

   Modified by CZ 26/05/01 for new mode execution.

   @return  Nonzero if instruction should NOT be executed                    */
/*---------------------------------------------------------------------------*/
static int
fetch (or1ksim *sim)
{
  static int break_just_hit = 0;

  if (NULL != sim->breakpoints)
    {
      /* MM: Check for breakpoint.  This has to be done in fetch cycle,
         because of peripheria.
         MM1709: if we cannot access the memory entry, we could not set the
         breakpoint earlier, so just check the breakpoint list.  */
      if (has_breakpoint (sim, peek_into_itlb (sim, sim->cpu_state.pc)) && !break_just_hit)
	{
	  break_just_hit = 1;
	  return 1;		/* Breakpoint set. */
	}
      break_just_hit = 0;
    }

  sim->breakpoint                 = 0;
  sim->cpu_state.iqueue.insn_addr = sim->cpu_state.pc;
  sim->cpu_state.iqueue.insn      = eval_insn (sim,sim->cpu_state.pc, &sim->breakpoint);

  /* Fetch instruction. */
  if (!sim->except_pending)
    {
      sim->runtime.cpu.instructions++;
    }

  /* update_pc will be called after execution */
  return 0;

}	/* fetch() */


/*---------------------------------------------------------------------------*/
/*!This code actually updates the PC value                                   */
/*---------------------------------------------------------------------------*/
static void
update_pc (or1ksim *sim)
{
  sim->cpu_state.delay_insn    = sim->next_delay_insn;
  sim->cpu_state.sprs[SPR_PPC] = sim->cpu_state.pc;	/* Store value for later */
  sim->cpu_state.pc            = sim->pcnext;
  sim->pcnext                  = sim->cpu_state.delay_insn ? sim->cpu_state.pc_delay :
                                                   sim->pcnext + 4;
}	/* update_pc() */


/*---------------------------------------------------------------------------*/
/*!Perform analysis of the instruction being executed

   This could be static for SIMPLE_EXECUTION, but made global for general use.

   @param[in] current  The instruction being executed                        */
/*---------------------------------------------------------------------------*/
void
analysis (or1ksim *sim,struct iqueue_entry *current)
{
  if (sim->config.cpu.dependstats)
    {
      /* Dynamic, dependency stats. */
      adddstats (sim, sim->cpu_state.icomplet.insn_index, current->insn_index, 1,
		 check_depend (sim, &sim->cpu_state.icomplet, current));

      /* Dynamic, functional units stats. */
      addfstats (sim, or32_opcodes[sim->cpu_state.icomplet.insn_index].func_unit,
		 or32_opcodes[current->insn_index].func_unit, 1,
		 check_depend (sim, &sim->cpu_state.icomplet, current));

      /* Dynamic, single stats. */
      addsstats (sim, current->insn_index, 1);
    }

  if (sim->config.cpu.superscalar)
    {
      if ((or32_opcodes[current->insn_index].func_unit == it_branch) ||
	  (or32_opcodes[current->insn_index].func_unit == it_jump))
	sim->runtime.sim.storecycles += 0;

      if (or32_opcodes[current->insn_index].func_unit == it_store)
	sim->runtime.sim.storecycles += 1;

      if (or32_opcodes[current->insn_index].func_unit == it_load)
	sim->runtime.sim.loadcycles += 1;

      /* Pseudo multiple issue benchmark */
      if ((sim->multissue[or32_opcodes[current->insn_index].func_unit] < 1) ||
	  (check_depend (sim, &sim->cpu_state.icomplet, current))
	  || (sim->issued_per_cycle < 1))
	{
	  int i;
	  for (i = 0; i < 20; i++)
	    sim->multissue[i] = 2;
	  sim->issued_per_cycle = 2;
	  sim->runtime.cpu.supercycles++;
	  if (check_depend (sim, &sim->cpu_state.icomplet, current))
	    sim->runtime.cpu.hazardwait++;
	  sim->multissue[it_unknown] = 2;
	  sim->multissue[it_shift] = 2;
	  sim->multissue[it_compare] = 1;
	  sim->multissue[it_branch] = 1;
	  sim->multissue[it_jump] = 1;
	  sim->multissue[it_extend] = 2;
	  sim->multissue[it_nop] = 2;
	  sim->multissue[it_move] = 2;
	  sim->multissue[it_movimm] = 2;
	  sim->multissue[it_arith] = 2;
	  sim->multissue[it_store] = 2;
	  sim->multissue[it_load] = 2;
	}
      sim->multissue[or32_opcodes[current->insn_index].func_unit]--;
      sim->issued_per_cycle--;
    }

  if (sim->config.cpu.dependstats)
    /* Instruction waits in completition buffer until retired. */
    memcpy (&sim->cpu_state.icomplet, current, sizeof (struct iqueue_entry));

  if (sim->config.sim.history)
    {
      /* History of execution */
      sim->hist_exec_tail = sim->hist_exec_tail->next;
      sim->hist_exec_tail->addr = sim->cpu_state.icomplet.insn_addr;
    }

  if (sim->config.sim.exe_log)
    dump_exe_log (sim);

}	/* analysis() */


#if !(DYNAMIC_EXECUTION)

/*---------------------------------------------------------------------------*/
/*!Store buffer analysis for store instructions

   Stores are accumulated and commited when IO is idle

   @param[in] cyc  Number of cycles being analysed                           */
/*---------------------------------------------------------------------------*/
static void
sbuf_store (or1ksim *sim,int cyc)
{
  int delta = sim->runtime.sim.cycles - sim->sbuf_prev_cycles;

  sim->sbuf_total_cyc   += cyc;
  sim->sbuf_prev_cycles  = sim->runtime.sim.cycles;

  /* Take stores from buffer, that occured meanwhile */
  while (sim->sbuf_count && delta >= sim->sbuf_buf[sim->sbuf_tail])
    {
      delta     -= sim->sbuf_buf[sim->sbuf_tail];
      sim->sbuf_tail  = (sim->sbuf_tail + 1) % MAX_SBUF_LEN;
      sim->sbuf_count--;
    }

  if (sim->sbuf_count)
    {
      sim->sbuf_buf[sim->sbuf_tail] -= delta;
    }

  /* Store buffer is full, take one out */
  if (sim->sbuf_count >= sim->config.cpu.sbuf_len)
    {
      sim->sbuf_wait_cyc          += sim->sbuf_buf[sim->sbuf_tail];
      sim->runtime.sim.mem_cycles += sim->sbuf_buf[sim->sbuf_tail];
      sim->sbuf_prev_cycles       += sim->sbuf_buf[sim->sbuf_tail];
      sim->sbuf_tail               = (sim->sbuf_tail + 1) % MAX_SBUF_LEN;
      sim->sbuf_count--;
    }

  /* Put newest store in the buffer */
  sim->sbuf_buf[sim->sbuf_head] = cyc;
  sim->sbuf_head           = (sim->sbuf_head + 1) % MAX_SBUF_LEN;
  sim->sbuf_count++;

}	/* sbuf_store() */


/*---------------------------------------------------------------------------*/
/*!Store buffer analysis for load instructions

   Previous stores should commit, before any load                            */
/*---------------------------------------------------------------------------*/
static void
sbuf_load (or1ksim *sim)
{
  int delta = sim->runtime.sim.cycles - sim->sbuf_prev_cycles;
  sim->sbuf_prev_cycles = sim->runtime.sim.cycles;

  /* Take stores from buffer, that occured meanwhile */
  while (sim->sbuf_count && delta >= sim->sbuf_buf[sim->sbuf_tail])
    {
      delta     -= sim->sbuf_buf[sim->sbuf_tail];
      sim->sbuf_tail  = (sim->sbuf_tail + 1) % MAX_SBUF_LEN;
      sim->sbuf_count--;
    }

  if (sim->sbuf_count)
    {
      sim->sbuf_buf[sim->sbuf_tail] -= delta;
    }

  /* Wait for all stores to complete */
  while (sim->sbuf_count > 0)
    {
      sim->sbuf_wait_cyc          += sim->sbuf_buf[sim->sbuf_tail];
      sim->runtime.sim.mem_cycles += sim->sbuf_buf[sim->sbuf_tail];
      sim->sbuf_prev_cycles       += sim->sbuf_buf[sim->sbuf_tail];
      sim->sbuf_tail               = (sim->sbuf_tail + 1) % MAX_SBUF_LEN;
      sim->sbuf_count--;
    }
}	/* sbuf_load() */

#endif	/* !DYNAMIC_EXECUTION */


/*---------------------------------------------------------------------------*/
/*!Outputs dissasembled instruction                                          */
/*---------------------------------------------------------------------------*/
void
dump_exe_log (or1ksim *sim)
{
  oraddr_t      insn_addr = sim->cpu_state.iqueue.insn_addr;
  unsigned int  i;
  unsigned int  j;
  uorreg_t      operand;

  if (insn_addr == 0xffffffff)
    {
      return;
    }

  if ((sim->config.sim.exe_log_start <= sim->runtime.cpu.instructions) &&
      ((sim->config.sim.exe_log_end <= 0) ||
       (sim->runtime.cpu.instructions <= sim->config.sim.exe_log_end)))
    {
      struct label_entry *entry;

      if (sim->config.sim.exe_log_marker &&
	  !(sim->runtime.cpu.instructions % sim->config.sim.exe_log_marker))
	{
	  fprintf (sim->runtime.sim.fexe_log,
		   "--------------------- %8lli instruction "
		   "---------------------\n",
		   sim->runtime.cpu.instructions);
	}

      switch (sim->config.sim.exe_log_type)
	{
	case EXE_LOG_HARDWARE:
	  fprintf (sim->runtime.sim.fexe_log,
		   "\nEXECUTED(%11llu): %" PRIxADDR ":  ",
		   sim->runtime.cpu.instructions, insn_addr);
	  fprintf (sim->runtime.sim.fexe_log, "%.2x%.2x",
		   eval_direct8 (sim,insn_addr, 0, 0),
		   eval_direct8 (sim,insn_addr + 1, 0, 0));
	  fprintf (sim->runtime.sim.fexe_log, "%.2x%.2x",
		   eval_direct8 (sim,insn_addr + 2, 0, 0),
		   eval_direct8 (sim,insn_addr + 3, 0, 0));

	  for (i = 0; i < MAX_GPRS; i++)
	    {
	      if (i % 4 == 0)
		{
		  fprintf (sim->runtime.sim.fexe_log, "\n");
		}

	      fprintf (sim->runtime.sim.fexe_log, "GPR%2u: %" PRIxREG "  ", i,
		       sim->cpu_state.reg[i]);
	    }

	  fprintf (sim->runtime.sim.fexe_log, "\n");
	  fprintf (sim->runtime.sim.fexe_log, "SR   : %.8" PRIx32 "  ",
		   sim->cpu_state.sprs[SPR_SR]);
	  fprintf (sim->runtime.sim.fexe_log, "EPCR0: %" PRIxADDR "  ",
		   sim->cpu_state.sprs[SPR_EPCR_BASE]);
	  fprintf (sim->runtime.sim.fexe_log, "EEAR0: %" PRIxADDR "  ",
		   sim->cpu_state.sprs[SPR_EEAR_BASE]);
	  fprintf (sim->runtime.sim.fexe_log, "ESR0 : %.8" PRIx32 "\n",
		   sim->cpu_state.sprs[SPR_ESR_BASE]);
	  break;

	case EXE_LOG_SIMPLE:
	case EXE_LOG_SOFTWARE:
	  disassemble_index (sim, sim->cpu_state.iqueue.insn,
			     sim->cpu_state.iqueue.insn_index);

	  entry = get_label (sim, insn_addr);
	  if (entry)
	    {
	      fprintf (sim->runtime.sim.fexe_log, "%s:\n", entry->name);
	    }

	  if (sim->config.sim.exe_log_type == EXE_LOG_SOFTWARE)
	    {
	      struct insn_op_struct *opd =
		sim->op_start[sim->cpu_state.iqueue.insn_index];

	      j = 0;
	      while (1)
		{
		  operand = eval_operand_val (sim->cpu_state.iqueue.insn, opd);
		  while (!(opd->type & OPTYPE_OP))
		    {
		      opd++;
		    }
		  if (opd->type & OPTYPE_DIS)
		    {
		      fprintf (sim->runtime.sim.fexe_log,
			       "EA =%" PRIxADDR " PA =%" PRIxADDR " ",
			       sim->cpu_state.insn_ea,
			       peek_into_dtlb (sim, sim->cpu_state.insn_ea, 0, 0));
		      opd++;	/* Skip of register operand */
		      j++;
		    }
		  else if ((opd->type & OPTYPE_REG) && operand)
		    {
		      fprintf (sim->runtime.sim.fexe_log, "r%-2i=%" PRIxREG " ",
			       (int) operand, evalsim_reg (sim, operand));
		    }
		  else
		    {
		      fprintf (sim->runtime.sim.fexe_log, "             ");
		    }
		  j++;
		  if (opd->type & OPTYPE_LAST)
		    {
		      break;
		    }
		  opd++;
		}
	      if (or32_opcodes[sim->cpu_state.iqueue.insn_index].flags & OR32_R_FLAG)
		{
		  fprintf (sim->runtime.sim.fexe_log, "SR =%" PRIxREG " ",
			   sim->cpu_state.sprs[SPR_SR]);
		  j++;
		}
	      while (j < 3)
		{
		  fprintf (sim->runtime.sim.fexe_log, "             ");
		  j++;
		}
	    }
	  fprintf (sim->runtime.sim.fexe_log, "%" PRIxADDR " ", insn_addr);
	  fprintf (sim->runtime.sim.fexe_log, "%s\n", sim->disassembled);
	}
    }
}	/* dump_exe_log() */


/*---------------------------------------------------------------------------*/
/*!Dump registers

   Supports the CLI 'r' and 't' commands                                     */
/*---------------------------------------------------------------------------*/
void
dumpreg (or1ksim *sim)
{
  int       i;
  oraddr_t  physical_pc;

  if ((physical_pc = peek_into_itlb (sim, sim->cpu_state.iqueue.insn_addr)))
    {
      disassemble_memory (sim,physical_pc, physical_pc + 4, 0);
    }
  else
    {
      PRINTF ("INTERNAL SIMULATOR ERROR:\n");
      PRINTF ("no translation for currently executed instruction\n");
    }

  // generate_time_pretty (temp, sim->runtime.sim.cycles * config.sim.clkcycle_ps);
  PRINTF (" (executed) [cycle %lld, #%lld]\n", sim->runtime.sim.cycles,
	  sim->runtime.cpu.instructions);
  if (sim->config.cpu.superscalar)
    {
      PRINTF ("Superscalar CYCLES: %u", sim->runtime.cpu.supercycles);
    }
  if (sim->config.cpu.hazards)
    {
      PRINTF ("  HAZARDWAIT: %u\n", sim->runtime.cpu.hazardwait);
    }
  else if (sim->config.cpu.superscalar)
    {
      PRINTF ("\n");
    }

  if ((physical_pc = peek_into_itlb (sim, sim->cpu_state.pc)))
    {
      disassemble_memory (sim,physical_pc, physical_pc + 4, 0);
    }
  else
    {
      PRINTF ("%" PRIxADDR ": : xxxxxxxx  ITLB miss follows", sim->cpu_state.pc);
    }

  PRINTF (" (next insn) %s", (sim->cpu_state.delay_insn ? "(delay insn)" : ""));

  for (i = 0; i < MAX_GPRS; i++)
    {
      if (i % 4 == 0)
	{
	  PRINTF ("\n");
	}

      PRINTF ("GPR%.2u: %" PRIxREG "  ", i, evalsim_reg (sim, i));
    }

  PRINTF ("flag: %u\n", sim->cpu_state.sprs[SPR_SR] & SPR_SR_F ? 1 : 0);

}	/* dumpreg() */


/*---------------------------------------------------------------------------*/
/*!Wrapper around real decode_execute function

   Some statistics here only

   @param[in] current  Instruction being executed                            */
/*---------------------------------------------------------------------------*/
static void
decode_execute_wrapper (or1ksim *sim,struct iqueue_entry *current)
{
  sim->breakpoint = 0;

#ifndef HAVE_EXECUTION
#error HAVE_EXECUTION has to be defined in order to execute programs.
#endif

  /* FIXME: Most of this file is not needed with DYNAMIC_EXECUTION */
#if !(DYNAMIC_EXECUTION)
  decode_execute (sim,current);
#endif

#if SET_OV_FLAG
  /* Check for range exception */
  if ((sim->cpu_state.sprs[SPR_SR] & SPR_SR_OVE) &&
      (sim->cpu_state.sprs[SPR_SR] & SPR_SR_OV))
    {
      except_handle (EXCEPT_RANGE, sim->cpu_state.sprs[SPR_EEAR_BASE]);
    }
#endif

  if (sim->breakpoint)
    {
      except_handle (sim,EXCEPT_TRAP, sim->cpu_state.sprs[SPR_EEAR_BASE]);
    }
}	/* decode_execute_wrapper() */

/*---------------------------------------------------------------------------*/
/*!Reset the CPU                                                             */
/*---------------------------------------------------------------------------*/
void
cpu_reset ( or1ksim *sim )
{
  int               i;
  struct hist_exec *hist_exec_head = NULL;
  struct hist_exec *hist_exec_new;

  sim->runtime.sim.cycles       = 0;
  sim->runtime.sim.loadcycles   = 0;
  sim->runtime.sim.storecycles  = 0;
  sim->runtime.cpu.instructions = 0;
  sim->runtime.cpu.supercycles  = 0;
  sim->runtime.cpu.hazardwait   = 0;

  for (i = 0; i < MAX_GPRS; i++)
    {
      setsim_reg (sim, i, 0);
    }

  memset (&sim->cpu_state.iqueue,   0, sizeof (sim->cpu_state.iqueue));
  memset (&sim->cpu_state.icomplet, 0, sizeof (sim->cpu_state.icomplet));

  sim->sbuf_head        = 0;
  sim->sbuf_tail        = 0;
  sim->sbuf_count       = 0;
  sim->sbuf_prev_cycles = 0;

  /* Initialise execution history circular buffer */
  for (i = 0; i < HISTEXEC_LEN; i++)
    {
      hist_exec_new = malloc (sizeof (struct hist_exec));

      if (!hist_exec_new)
	{
	  fprintf (stderr, "Out-of-memory\n");
	  exit (1);
	}

      if (!hist_exec_head)
	{
	  hist_exec_head = hist_exec_new;
	}
      else
	{
	  sim->hist_exec_tail->next = hist_exec_new;
	}

      hist_exec_new->prev = sim->hist_exec_tail;
      sim->hist_exec_tail = hist_exec_new;
    }

  /* Make hist_exec_tail->next point to hist_exec_head */
  sim->hist_exec_tail->next = hist_exec_head;
  hist_exec_head->prev = sim->hist_exec_tail;

  /* MM1409: All progs should start at reset vector entry! This sorted out by
     setting the sim->cpu_state.pc field below. Not clear this is very good code! */

  /* Patches suggested by Shinji Wakatsuki, so that the vector address takes
     notice of the Exception Prefix High bit of the Supervision register */
  sim->pcnext = (sim->cpu_state.sprs[SPR_SR] & SPR_SR_EPH ? 0xf0000000 : 0x00000000);

  if (sim->config.sim.verbose)
    {
      PRINTF ("Starting at 0x%" PRIxADDR "\n", sim->pcnext);
    }

  sim->cpu_state.pc  = sim->pcnext;
  sim->pcnext       += 4;

  /* MM1409: All programs should set their stack pointer!  */
#if !(DYNAMIC_EXECUTION)
  except_handle (sim,EXCEPT_RESET, 0);
  update_pc (sim);
#endif

  sim->except_pending = 0;
  sim->cpu_state.pc   = sim->cpu_state.sprs[SPR_SR] & SPR_SR_EPH ?
    0xf0000000 + EXCEPT_RESET : EXCEPT_RESET;

}	/* cpu_reset() */


/*---------------------------------------------------------------------------*/
/*!Simulates one CPU clock cycle

  @return  non-zero if a breakpoint is hit, zero otherwise.                  */
/*---------------------------------------------------------------------------*/
int
cpu_clock ( or1ksim *sim )
{
  sim->except_pending  = 0;
  sim->next_delay_insn = 0;

  if (fetch ( sim ))
    {
      PRINTF ("Breakpoint hit.\n");
      return  1;
    }

  if (sim->except_pending)
    {
      update_pc (sim);
      sim->except_pending = 0;
      return  0;
    }

  if (sim->breakpoint)
    {
      except_handle (sim,EXCEPT_TRAP, sim->cpu_state.sprs[SPR_EEAR_BASE]);
      update_pc (sim);
      sim->except_pending = 0;
      return  0;
    }

  decode_execute_wrapper (sim,&sim->cpu_state.iqueue);
  update_pc (sim);
  return  0;

}	/* cpu_clock() */


/*---------------------------------------------------------------------------*/
/*!If decoding cannot be found, call this function                           */
/*---------------------------------------------------------------------------*/
#if SIMPLE_EXECUTION
void
l_invalid (or1ksim *sim,struct iqueue_entry *current)
{
#else
void
l_invalid (or1ksim *sim)
{
#endif
  except_handle (sim,EXCEPT_ILLEGAL, sim->cpu_state.iqueue.insn_addr);

}	/* l_invalid() */


/*---------------------------------------------------------------------------*/
/*!The main execution loop                                                   */
/*---------------------------------------------------------------------------*/
void
exec_main ( or1ksim *sim )
{
  long long time_start;

  while (1)
    {
      time_start = sim->runtime.sim.cycles;
      if (sim->config.debug.enabled)
	{
	  while (sim->runtime.cpu.stalled)
	    {
	      if (sim->config.debug.rsp_enabled)
		{
		  handle_rsp ();
		}
	      else if (sim->config.debug.gdb_enabled)
		{
		  block_jtag (sim);
		  handle_server_socket (sim, FALSE);
		}
	      else
		{
		  fprintf (stderr, "ERROR: CPU stalled and GDB connection not "
			   "enabled: Invoking CLI and terminating.\n");
		  /* Dump the user into interactive mode.  From there he or
		     she can decide what to do. */
		  handle_sim_command ( sim );
		  sim_done (sim);
		}
	      if (sim->runtime.sim.iprompt)
		handle_sim_command ( sim );
	    }
	}

      /* Each cycle has counter of mem_cycles; this value is joined with cycles
         at the end of the cycle; no sim originated memory accesses should be
         performed inbetween. */
      sim->runtime.sim.mem_cycles = 0;

      if (!sim->config.pm.enabled ||
	  !(sim->config.pm.enabled &
	    (sim->cpu_state.sprs[SPR_PMR] & (SPR_PMR_DME | SPR_PMR_SME))))
	{
	  if (cpu_clock (sim))
	    {
	      /* A breakpoint has been hit, drop to interactive mode */
	      handle_sim_command ( sim );
	    }
	}

      if (sim->config.vapi.enabled && sim->runtime.vapi.enabled)
	{
	  vapi_check ();
	}

      if (sim->config.debug.gdb_enabled)
	{
	  handle_server_socket (sim, FALSE);	/* block & check_stdin = false */
	}

      if (sim->config.debug.enabled)
	{
	  if (sim->cpu_state.sprs[SPR_DMR1] & SPR_DMR1_ST)
	    {
	      set_stall_state (sim, 1);

	      if (sim->config.debug.rsp_enabled)
		{
		  rsp_exception (EXCEPT_TRAP);
		}
	    }
	}

      sim->runtime.sim.cycles        += sim->runtime.sim.mem_cycles;
      sim->scheduler.job_queue->time -= sim->runtime.sim.cycles - time_start;

      if (sim->scheduler.job_queue->time <= 0)
	{
	  do_scheduler (sim);
	}
    }
}	/* exec_main() */

#if COMPLEX_EXECUTION

/* Include generated/built in decode_execute function */
#include "execgen.c"

#elif SIMPLE_EXECUTION


/*---------------------------------------------------------------------------*/
/*!Evaluates source operand

   Implementation specific.

   @param[in] op_no       The operand
   @param[in] insn_index  Address of the instruction
   @param[in] insn        The instruction

   @return  The value of the operand                                         */
/*---------------------------------------------------------------------------*/
static uorreg_t
eval_operand (int            op_no,
	      unsigned long  insn_index,
	      uint32_t       insn)
{
  struct insn_op_struct *opd = op_start[insn_index];
  uorreg_t               ret;

  while (op_no)
    {
      if (opd->type & OPTYPE_LAST)
	{
	  fprintf (stderr,
		   "Instruction requested more operands than it has\n");
	  exit (1);
	}

      if ((opd->type & OPTYPE_OP) && !(opd->type & OPTYPE_DIS))
	{
	  op_no--;
	}

      opd++;
    }

  if (opd->type & OPTYPE_DIS)
    {
      ret = eval_operand_val (insn, opd);

      while (!(opd->type & OPTYPE_OP))
	{
	  opd++;
	}

      opd++;
      ret               += evalsim_reg (eval_operand_val (insn, opd));
      sim->cpu_state.insn_ea  = ret;

      return  ret;
    }

  if (opd->type & OPTYPE_REG)
    {
      return  evalsim_reg (eval_operand_val (insn, opd));
    }

  return  eval_operand_val (insn, opd);

}	/* eval_operand() */


/*---------------------------------------------------------------------------*/
/*!Set destination operand (register direct) with value.

   Implementation specific.

   @param[in] op_no       The operand
   @param[in] value       The value to set
   @param[in] insn_index  Address of the instruction
   @param[in] insn        The instruction                                    */
/*---------------------------------------------------------------------------*/
static void
set_operand (int            op_no,
	     orreg_t        value,
	     unsigned long  insn_index,
	     uint32_t       insn)
{
  struct insn_op_struct *opd = op_start[insn_index];

  while (op_no)
    {
      if (opd->type & OPTYPE_LAST)
	{
	  fprintf (stderr,
		   "Instruction requested more operands than it has\n");
	  exit (1);
	}

      if ((opd->type & OPTYPE_OP) && !(opd->type & OPTYPE_DIS))
	{
	  op_no--;
	}

      opd++;
    }

  if (!(opd->type & OPTYPE_REG))
    {
      fprintf (stderr, "Trying to set a non-register operand\n");
      exit (1);
    }

  setsim_reg (eval_operand_val (insn, opd), value);

}	/* set_operand() */


/*---------------------------------------------------------------------------*/
/*!Simple and rather slow decoding function

   Based on built automata.

   @param[in] current  The current instruction to execute                    */
/*---------------------------------------------------------------------------*/
static void
decode_execute (struct iqueue_entry *current)
{
  int insn_index;

  current->insn_index = insn_index = insn_decode (current->insn);

  if (insn_index < 0)
    {
      l_invalid (sim,current);
    }
  else
    {
      or32_opcodes[insn_index].exec (current);
    }

  if (do_stats)
    analysis (&sim->cpu_state.iqueue);
}

#include "insnset.c"

#elif defined(DYNAMIC_EXECUTION)

#else
# error "Must define SIMPLE_EXECUTION, COMPLEX_EXECUTION or DYNAMIC_EXECUTION"
#endif
