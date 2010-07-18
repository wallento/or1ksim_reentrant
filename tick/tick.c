/* tick.c -- Simulation of OpenRISC 1000 tick timer

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
   Copyright (C) 2005 György `nog' Jeney, nog@sdf.lonestar.org
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

/* This is functional simulation of OpenRISC 1000 architectural tick timer */

/* Autoconf and/or portability configuration */
#include "config.h"
#include "port.h"

/* System includes */
#include <stdlib.h>
#include <stdio.h>

/* Package includes */
#include "arch.h"
#include "abstract.h"
#include "except.h"
#include "tick.h"
#include "opcode/or32.h"
#include "spr-defs.h"
#include "execute.h"
#include "pic.h"
#include "sprs.h"
#include "sim-config.h"
#include "sched.h"

/*! Reset. It initializes TTCR register. */
void
tick_reset (or1ksim *sim)
{
  if (sim->config.sim.verbose)
    {
      PRINTF ("Resetting Tick Timer.\n");
    }

  sim->cpu_state.sprs[SPR_TTCR] = 0;
  sim->cpu_state.sprs[SPR_TTMR] = 0;
  sim->tick_count = 0;
}

/*! Raises a timer exception */
static void
tick_raise_except (or1ksim *sim,void *dat)
{
  sim->cpu_state.sprs[SPR_TTMR] |= SPR_TTMR_IP;

  /* Reschedule unconditionally, since we have to raise the exception until
   * TTMR_IP has been cleared */
  sched_next_insn (sim, tick_raise_except, NULL);

  /* be sure not to issue a timer exception if an exception occured before it */
  if (sim->cpu_state.sprs[SPR_SR] & SPR_SR_TEE)
    {
      except_handle (sim,EXCEPT_TICK, sim->cpu_state.sprs[SPR_EEAR_BASE]);
    }
}

/*! Restarts the tick timer */
static void
tick_restart (or1ksim *sim,void *dat)
{
  sim->cpu_state.sprs[SPR_TTCR] = 0;
  sim->cycles_start = sim->runtime.sim.cycles;
  SCHED_ADD (tick_restart, NULL, sim->cpu_state.sprs[SPR_TTMR] & SPR_TTMR_PERIOD);
}

/*! Stops the timer */
static void
tick_one_shot (or1ksim *sim,void *dat)
{
  sim->cpu_state.sprs[SPR_TTCR] = sim->cpu_state.sprs[SPR_TTMR] & SPR_TTMR_PERIOD;
  sim->tick_count = 0;
}

/*! Schedules the timer jobs */
static void
sched_timer_job (or1ksim *sim, uorreg_t prev_ttmr)
{
  uorreg_t ttmr = sim->cpu_state.sprs[SPR_TTMR];
  uint32_t match_time = ttmr & SPR_TTMR_PERIOD;
  uint32_t ttcr_period = spr_read_ttcr (sim) & SPR_TTCR_PERIOD;

  /* Remove previous jobs if they exists */
  if ((prev_ttmr & SPR_TTMR_IE) && !(ttmr & SPR_TTMR_IP))
    {
      SCHED_FIND_REMOVE (tick_raise_except, NULL);
    }

  switch (prev_ttmr & SPR_TTMR_M)
    {
    case SPR_TTMR_RT:
      SCHED_FIND_REMOVE (tick_restart, NULL);
      break;

    case SPR_TTMR_SR:
      SCHED_FIND_REMOVE (tick_one_shot, NULL);
      break;
    }

  if (match_time >= ttcr_period)
    {
      match_time -= ttcr_period;
    }
  else
    {
      match_time += (0xfffffffu - ttcr_period) + 1;
    }

  switch (ttmr & SPR_TTMR_M)
    {
    case 0:			/* Disabled timer */
      if (!match_time && (ttmr & SPR_TTMR_IE) && !(ttmr & SPR_TTMR_IP))
	SCHED_ADD (tick_raise_except, NULL, 0);
      break;

    case SPR_TTMR_RT:		/* Auto-restart timer */
      SCHED_ADD (tick_restart, NULL, match_time);
      if ((ttmr & SPR_TTMR_IE) && !(ttmr & SPR_TTMR_IP))
	SCHED_ADD (tick_raise_except, NULL, match_time);
      break;

    case SPR_TTMR_SR:		/* One-shot timer */
      if (sim->tick_count)
	{
	  SCHED_ADD (tick_one_shot, NULL, match_time);
	  if ((ttmr & SPR_TTMR_IE) && !(ttmr & SPR_TTMR_IP))
	    SCHED_ADD (tick_raise_except, NULL, match_time);
	}
      break;

    case SPR_TTMR_CR:		/* Continuos timer */
      if ((ttmr & SPR_TTMR_IE) && !(ttmr & SPR_TTMR_IP))
	SCHED_ADD (tick_raise_except, NULL, match_time);
    }
}


/*! Handles a write to the ttcr spr */
void
spr_write_ttcr (or1ksim *sim, uorreg_t value)
{
  sim->cycles_start = sim->runtime.sim.cycles - value;

  sched_timer_job (sim, sim->cpu_state.sprs[SPR_TTMR]);
}

/*! Value is the *previous* value of SPR_TTMR.  The new one can be found in
    sim->cpu_state.sprs[SPR_TTMR] */
void
spr_write_ttmr (or1ksim *sim, uorreg_t prev_val)
{
  uorreg_t value = sim->cpu_state.sprs[SPR_TTMR];

  /* Code running on or1k can't set SPR_TTMR_IP so make sure it isn't */
  sim->cpu_state.sprs[SPR_TTMR] &= ~SPR_TTMR_IP;

  /* If the timer was already disabled, ttcr should not be updated */
  if (sim->tick_count)
    {
      sim->cpu_state.sprs[SPR_TTCR] = sim->runtime.sim.cycles - sim->cycles_start;
    }

  sim->cycles_start = sim->runtime.sim.cycles - sim->cpu_state.sprs[SPR_TTCR];

  sim->tick_count = value & SPR_TTMR_M;

  if ((sim->tick_count == 0xc0000000) &&
      (sim->cpu_state.sprs[SPR_TTCR] == (value & SPR_TTMR_PERIOD)))
    {
      sim->tick_count = 0;
    }

  sched_timer_job (sim, prev_val);
}

uorreg_t
spr_read_ttcr (or1ksim *sim)
{
  uorreg_t ret;

  if (!sim->tick_count)
    {
      /* Report the time when the counter stoped (and don't carry on
	 counting) */
      ret = sim->cpu_state.sprs[SPR_TTCR];
    }
  else
    {
      ret = sim->runtime.sim.cycles - sim->cycles_start;
    }

  return  ret;
}
