/* sim-config.h -- Simulator configuration header file

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
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


#ifndef SIM_CONFIG_H
#define SIM_CONFIG_H

/* System includes */
#include <stdio.h>

/* Package includes */
#include "arch.h"
#include "siminstance.h"


/*! Union of all possible paramter values */
union param_val
{
  char          *str_val;
  int            int_val;
  long long int  longlong_val;
  oraddr_t       addr_val;
};

/*! Enum of all possible paramter types */
enum param_t
{
  paramt_none = 0,		/* No parameter */
  paramt_str,			/* String parm enclosed in double quotes (") */
  paramt_word,			/* String parm NOT enclosed in double quotes */
  paramt_int,			/* Integer parameter */
  paramt_longlong,		/* Long long int parameter */
  paramt_addr			/* Address parameter */
};

/* Prototypes for external use */
extern void  set_config_command (or1ksim *sim, int argc, char **argv);
extern void  init_defconfig (or1ksim *sim);
extern int   parse_args (or1ksim *sim, int argc, char *argv[]);
extern void  print_config (or1ksim *sim);
extern void  reg_config_param (struct config_section *sec,
			       const char            *param,
			       enum param_t           type,
			       void (*param_cb)  (or1ksim *sim,union param_val,
						  void *));
extern struct config_section *reg_config_sec (or1ksim* sim, const char *section,
					      void *(*sec_start) (or1ksim* sim),
					      void  (*sec_end) (or1ksim* sim, void *));

extern void  reg_config_secs (or1ksim *sim);

#endif /* SIM_CONFIG_H */
