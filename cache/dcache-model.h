/* dcache-model.h -- data cache header file

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


#ifndef DCACHE_MODEL__H
#define DCACHE_MODEL__H

/* Package includes */
#include "sim-config.h"
#include "siminstance.h"

/* Prototypes for external use */

extern uint32_t  dc_simulate_read (or1ksim *sim,oraddr_t  dataaddr,
				   oraddr_t  virt_addr,
				   int       width);
extern void      dc_simulate_write (or1ksim *sim,oraddr_t  dataaddr,
				    oraddr_t  virt_addr,
				    uint32_t  data,
				    int       width);
extern void      dc_info (or1ksim *sim);
extern void      dc_inv (or1ksim *sim,oraddr_t dataaddr);
extern void      reg_dc_sec (or1ksim* sim);

#endif	/* DCACHE_MODEL__H */
