/* 16450.h -- Definition of types and structures for 8250/16450 serial UART

   Copyright (C) 2000 Damjan Lampret, lampret@opencores.org
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


#ifndef UART_16450__H
#define UART_16450__H

/* Prototypes for external use*/
extern void  uart_reset (or1ksim *sim,void *dat);
extern void  uart_status (or1ksim* sim, void *dat);
extern void  reg_uart_sec (or1ksim* sim);

#endif  /* UART_16450__H */
