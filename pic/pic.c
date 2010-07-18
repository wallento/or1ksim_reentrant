/* pic.c -- Simulation of OpenRISC 1000 programmable interrupt controller

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
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


/* Autoconf and/or portability configuration */
#include "config.h"
#include "port.h"

/* System includes */
#include <stdlib.h>
#include <stdio.h>

/* Package includes */
#include "arch.h"
#include "abstract.h"
#include "pic.h"
#include "opcode/or32.h"
#include "spr-defs.h"
#include "execute.h"
#include "except.h"
#include "sprs.h"
#include "sim-config.h"
#include "sched.h"


/* Reset. It initializes PIC registers. */
void
pic_reset (or1ksim *sim)
{
  PRINTF ("Resetting PIC.\n");
  sim->cpu_state.sprs[SPR_PICMR] = 0;
  sim->cpu_state.sprs[SPR_PICPR] = 0;
  sim->cpu_state.sprs[SPR_PICSR] = 0;
}

/* Handles the reporting of an interrupt if it had to be delayed */
static void
pic_rep_int (or1ksim *sim,void *dat)
{
  if (sim->cpu_state.sprs[SPR_PICSR])
    {
      except_handle (sim,EXCEPT_INT, sim->cpu_state.sprs[SPR_EEAR_BASE]);
    }
}

/* Called whenever interrupts get enabled */
void
pic_ints_en (or1ksim *sim)
{
  if ((sim->cpu_state.sprs[SPR_PICMR] & sim->cpu_state.sprs[SPR_PICSR]))
    SCHED_ADD (pic_rep_int, NULL, 0);
}

/* Asserts interrupt to the PIC. */
/* WARNING: If this is called during a simulated instruction (ie. from a read/
 * write mem callback), the interrupt will be delivered after the instruction
 * has finished executeing */
void
report_interrupt (or1ksim *sim,int line)
{
  uint32_t lmask = 1 << line;

  /* Disable doze and sleep mode */
  sim->cpu_state.sprs[SPR_PMR] &= ~(SPR_PMR_DME | SPR_PMR_SME);

  /* If PIC is disabled, don't set any register, just raise EXCEPT_INT */
  if (!sim->config.pic.enabled)
    {
      if (sim->cpu_state.sprs[SPR_SR] & SPR_SR_IEE)
	except_handle (sim, EXCEPT_INT, sim->cpu_state.sprs[SPR_EEAR_BASE]);
      return;
    }

  if (sim->cpu_state.pic_lines & lmask)
    {
      /* No edge occured, warn about performance penalty and exit */
      fprintf (stderr, "Warning: Int line %d did not change state\n", line);
      return;
    }

  sim->cpu_state.pic_lines |= lmask;
  sim->cpu_state.sprs[SPR_PICSR] |= lmask;

  if ((sim->cpu_state.sprs[SPR_PICMR] & lmask) || line < 2)
    if (sim->cpu_state.sprs[SPR_SR] & SPR_SR_IEE)
      SCHED_ADD (pic_rep_int, NULL, 0);
}

/* Clears an int on a pic line */
void
clear_interrupt (or1ksim *sim,int line)
{
  sim->cpu_state.pic_lines &= ~(1 << line);

  if (!sim->config.pic.edge_trigger)
    sim->cpu_state.sprs[SPR_PICSR] &= ~(1 << line);
}

/*----------------------------------------------------[ PIC configuration ]---*/


/*---------------------------------------------------------------------------*/
/*!Enable or disable the programmable interrupt controller

   Set the corresponding field in the UPR

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
pic_enabled (or1ksim *sim,union param_val  val,
	     void            *dat)
{
  if (val.int_val)
    {
      sim->cpu_state.sprs[SPR_UPR] |= SPR_UPR_PICP;
    }
  else
    {
      sim->cpu_state.sprs[SPR_UPR] &= ~SPR_UPR_PICP;
    }

  sim->config.pic.enabled = val.int_val;

}	/* pic_enabled() */


/*---------------------------------------------------------------------------*/
/*!Enable or disable edge triggering of interrupts

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
pic_edge_trigger (or1ksim *sim,union param_val  val,
		  void            *dat)
{
  sim->config.pic.edge_trigger = val.int_val;

}	/* pic_edge_trigger() */


/*---------------------------------------------------------------------------*/
/*!Initialize a new interrupt controller configuration

   ALL parameters are set explicitly to default values in init_defconfig()   */
/*---------------------------------------------------------------------------*/
void
reg_pic_sec (or1ksim *sim)
{
  struct config_section *sec = reg_config_sec (sim, "pic", NULL, NULL);

  reg_config_param (sec, "enabled",      paramt_int, pic_enabled);
  reg_config_param (sec, "edge_trigger", paramt_int, pic_edge_trigger);

}	/* reg_pic_sec() */
