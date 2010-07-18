/* immu.h -- Instruction MMU header file

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


#ifndef IMMU__H
#define IMMU__H

/* Package includes */
#include "sim-config.h"
#include "siminstance.h"

struct immu
{
  int       enabled;		/* Whether IMMU is enabled */
  int       nways;		/* Number of ITLB ways */
  int       nsets;		/* Number of ITLB sets */
  oraddr_t  pagesize;		/* ITLB page size */
  int       pagesize_log2;	/* ITLB page size (log2(pagesize)) */
  oraddr_t  page_offset_mask;	/* Address mask to get page offset */
  oraddr_t  page_mask;		/* Page number mask (diff. from vpn) */
  oraddr_t  vpn_mask;		/* Address mask to get vpn */
  int       lru_reload;		/* What to reload the lru value to */
  oraddr_t  set_mask;		/* Mask to get set of an address */
  int       entrysize;		/* ITLB entry size */
  int       ustates;		/* number of ITLB usage states */
  int       missdelay;		/* How much cycles does the miss cost */
  int       hitdelay;		/* How much cycles does the hit cost */
};

#define IADDR_PAGE(addr) ((addr) & sim->immu_state->page_mask)

/* Prototypes for external use */

extern oraddr_t  immu_translate (or1ksim *sim,oraddr_t virtaddr);
extern oraddr_t  immu_simulate_tlb (oraddr_t virtaddr);
extern oraddr_t  peek_into_itlb (or1ksim *sim, oraddr_t virtaddr);
extern void      reg_immu_sec (or1ksim *sim);

#endif /* IMMU__H */
