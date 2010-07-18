/* or1ksim.h -- Simulator library header file

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


/* Header file definining the interface to the Or1ksim library. */


#ifndef OR1KSIM__H
#define OR1KSIM__H

#include "siminstance.h"

/* The bus width */

/* #define BUSWIDTH  32 */

/* The return codes */

enum  or1ksim_rc {
  OR1KSIM_RC_OK,		/* No error */

  OR1KSIM_RC_BADINIT,		/* Couldn't initialize */
  OR1KSIM_RC_BRKPT		/* Hit a breakpoint */
};

/* The interface methods */

#ifdef __cplusplus
extern "C" {
#endif

or1ksim* or1ksim_instance();

int  or1ksim_init( or1ksim* instance,
		   const char         *config_file,
		   const char         *image_file,
		   void               *class_ptr,
		   unsigned long int (*upr)( void              *class_ptr,
					     unsigned long int  addr,
					     unsigned long int  mask),
		   void              (*upw)( void              *class_ptr,
					     unsigned long int  addr,
					     unsigned long int  mask,
					     unsigned long int  wdata ) );

int  or1ksim_run( or1ksim *sim, double  duration );

void  or1ksim_reset_duration( or1ksim *sim, double duration );

void  or1ksim_set_time_point( or1ksim *sim );

double  or1ksim_get_time_period( or1ksim *sim );

int  or1ksim_is_le( or1ksim *sim );

unsigned long int  or1ksim_clock_rate( or1ksim *sim );

void or1ksim_interrupt( or1ksim *sim, int  i );

void or1ksim_interrupt_set( or1ksim *sim, int  i );

void or1ksim_interrupt_clear( or1ksim *sim, int  i );

/* new library routines */


#ifdef __cplusplus
}
#endif


#endif	/* OR1KSIM__H */
