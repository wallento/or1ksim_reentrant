/* debug-unit.h -- Simulation of Or1k debug unit

   Copyright (C) 2001 Chris Ziomkowski, chris@asics.ws
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


#ifndef DEBUG_UNIT__H
#define DEBUG_UNIT__H

#include "siminstance.h"

/*! The possible debug unit actions */
enum debug_unit_action
{
  DebugInstructionFetch = 1,
  DebugLoadAddress      = 2,
  DebugStoreAddress     = 3,
  DebugLoadData         = 4,
  DebugStoreData        = 5
};

/* Function prototypes for external use */
extern void  du_reset (or1ksim *sim);
extern void  set_stall_state (or1ksim *sim, int state);
extern int   check_debug_unit (or1ksim *sim,enum debug_unit_action  action,
			       unsigned long           udata);
extern int   debug_get_register (or1ksim *sim,oraddr_t  address,
				 uorreg_t *data);
extern int   debug_set_register (or1ksim *sim,oraddr_t  address,
				 uorreg_t  data);
extern int   debug_set_chain (or1ksim *sim, enum debug_scan_chain_ids  chain);
extern int   debug_ignore_exception (or1ksim *sim,unsigned long except);
extern void  reg_debug_sec (or1ksim *sim);

#endif	/*  DEBUG_UNIT__H */
