/* parse.h -- Header file for parse.c

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

/* Here we define some often used caharcters in assembly files.  This wil
   probably go into architecture dependent directory. */


#ifndef PARSE__H
#define PARSE__H

/* Package includes */
#include "sim-config.h"
#include "siminstance.h"

/* Function prototypes for external use */
char     *strstrip (char       *dst,
		    const char *src,
		    int         n);
uint32_t  loadcode (or1ksim *sim,char     *filename,
		    oraddr_t  startaddr,
		    oraddr_t  virtphy_transl);

#endif	/* PARSE__H */
