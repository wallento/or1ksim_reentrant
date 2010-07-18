/* stats.h -- Header file for stats.c

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


#ifndef STATS__H
#define STATS__H

/* Package includes */
#include "opcode/or32.h"

#include "siminstance.h"

/* Function prototypes for external use */
extern void addsstats (or1ksim *sim, int item, int cnt_dynamic);
extern void adddstats (or1ksim *sim, int item1, int item2, int cnt_dynamic, int depend);
extern void addfstats (or1ksim *sim, enum insn_type item1, enum insn_type item2,
		       int cnt_dynamic, int depend);
extern void initstats (or1ksim *sim);
extern void printstats (or1ksim *sim, int which);

#endif	/*  STATS__H */
