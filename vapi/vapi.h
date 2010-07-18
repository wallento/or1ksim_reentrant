/* vapi.h - Verification API Interface

   Copyright (C) 2001, Marko Mlinar, markom@opencores.org
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


#ifndef  VAPI__H
#define  VAPI__H

/*! Maximum value for VAPI device id */
#define VAPI_MAX_DEVID 0xFFFF

/* Types of commands that can be written to the VAPI log file */
typedef enum
{
  VAPI_COMMAND_REQUEST = 0,	/* Data coming from outside world to device */
  VAPI_COMMAND_SEND = 1,	/* Device writing data to the outside world */
  VAPI_COMMAND_END = 2		/* End of log for device */
} VAPI_COMMAND;

/* Prototypes for external use */
extern int   vapi_init (or1ksim *sim);
extern void  vapi_done (or1ksim *sim);
extern void  vapi_install_handler (or1ksim *sim, unsigned long id,
				   void (*read_func) (or1ksim *sim,unsigned long,
						      unsigned long,
						      void *),
				  void *dat);
extern void  vapi_install_multi_handler (or1ksim *sim, unsigned long base_id,
					 unsigned long num_ids,
					 void (*read_func) (or1ksim *sim,unsigned long,
							    unsigned long,
							    void *),
					 void *dat);
extern void  vapi_check ();
extern int   vapi_num_unconnected (or1ksim *sim, int printout);
extern void  vapi_send (or1ksim *sim, unsigned long id,
			unsigned long data);
extern void  vapi_write_log_file (or1ksim *sim, VAPI_COMMAND command,
				  unsigned long device_id,
				  unsigned long data);
extern void  reg_vapi_sec (or1ksim *sim);

#endif /* VAPI__H */
