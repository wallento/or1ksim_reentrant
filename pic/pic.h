/* pic.h -- Definition of types and structures for openrisc 1000 PIC

   Copyright (C) 2000 Damjan Lampret, lampret@opencores.org
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


#ifndef PIC__H
#define PIC__H

#include "siminstance.h"

/* Prototypes */
extern void  pic_reset (or1ksim *sim);
extern void  report_interrupt (or1ksim *sim,int line);
extern void  clear_interrupt (or1ksim *sim,int line);
extern void  pic_ints_en (or1ksim *sim);
extern void  reg_pic_sec (or1ksim *sim);

#endif  /* PIC__H */
