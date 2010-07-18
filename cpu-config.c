/* cpu-config.c -- CPU configuration

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

/* Broken out from sim-config.c */


/* Autoconf and/or portability configuration */
#include "config.h"

/* System includes */
#include <stdio.h>

/* Package includes */
#include "cpu-config.h"
#include "sim-config.h"
#include "spr-defs.h"
#include "execute.h"


#define WARNING(s) fprintf (stderr, "Warning: config.%s: %s\n", sim->cur_section->name, (s))

/*---------------------------------------------------------------------------*/
/*!Set the CPU version

   Value must be an 8-bit integer. Larger values are truncated with a warning.

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
cpu_ver (or1ksim *sim,union param_val  val,
	 void            *dat)
{
  if (val.int_val > 0xff)
    {
      WARNING ("CPU version > 8 bits truncated\n");
    }

  sim->cpu_state.sprs[SPR_VR] &= ~SPR_VR_VER;
  sim->cpu_state.sprs[SPR_VR] |= (val.int_val & 0xff) << SPR_VR_VER_OFF;

}	/* cpu_ver() */


/*---------------------------------------------------------------------------*/
/*!Set the CPU configuration

   Value must be an 8-bit integer. Larger values are truncated with a warning.

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
cpu_cfg (or1ksim *sim,union param_val  val,
	 void            *dat)
{
  if (val.int_val > 0xff)
    {
      WARNING ("CPU configuration > 8 bits truncated\n");
    }

  sim->cpu_state.sprs[SPR_VR] &= ~SPR_VR_CFG;
  sim->cpu_state.sprs[SPR_VR] |= (val.int_val & 0xff) << SPR_VR_CFG_OFF;

}	/* cpu_cfg() */


/*---------------------------------------------------------------------------*/
/*!Set the CPU revision

   Value must be an 6-bit integer. Larger values are truncated with a warning.


   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
cpu_rev (or1ksim *sim,union param_val  val,
	 void            *dat)
{
  if (val.int_val > 0x3f)
    {
      WARNING ("CPU revision > 6 bits truncated\n");
    }

  sim->cpu_state.sprs[SPR_VR] &= ~SPR_VR_REV_OFF ;
  sim->cpu_state.sprs[SPR_VR] |= (val.int_val & 0x3f) << SPR_VR_REV_OFF ;

}	/* cpu_rev() */


static void
cpu_upr (or1ksim *sim,union param_val val, void *dat)
{
  sim->cpu_state.sprs[SPR_UPR] = val.int_val;
}

/*---------------------------------------------------------------------------*/
/*!Set the CPU configuration

   Value must be just the OB32S instruction set bit. Nothing else is currently
   supported. If other values are specified, they will be set, but with a
   warning.


   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
cpu_cfgr (or1ksim *sim,union param_val  val,
	  void            *dat)
{
  if (SPR_CPUCFGR_OB32S != val.int_val)
    {
      WARNING ("CPU configuration: only OB32S currently supported\n");
    }

  sim->cpu_state.sprs[SPR_CPUCFGR] = val.int_val;

}	/* cpu_cfgr() */


/*---------------------------------------------------------------------------*/
/*!Set the CPU supervision register

   Only the lowest 17 bits may be set. The top 4 bits are for context ID's
   (not currently supported), the rest are reserved and should not be set.

   If such values are specified, the value will be set (it has no effect), but
   with a warning.

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
cpu_sr (or1ksim *sim,union param_val  val,
	void            *dat)
{
  if (0 != (val.int_val & 0xf0000000))
    {
      WARNING ("Supervision Register ContextID not supported: ignored\n");
    }
  else if (val.int_val > 0x1ffff)
    {
      WARNING ("Supervision Register reserved bits set: ignored\n");
    }

  sim->cpu_state.sprs[SPR_SR] = val.int_val;

}	/* cpu_sr() */


static void
cpu_hazards (or1ksim *sim, union param_val val, void *dat)
{
  sim->config.cpu.hazards = val.int_val;
}

static void
cpu_superscalar (or1ksim *sim, union param_val val, void *dat)
{
  sim->config.cpu.superscalar = val.int_val;
}

static void
cpu_dependstats (or1ksim *sim, union param_val val, void *dat)
{
  sim->config.cpu.dependstats = val.int_val;
}

static void
cpu_sbuf_len (or1ksim *sim, union param_val val, void *dat)
{
  if (val.int_val >= MAX_SBUF_LEN)
    {
      sim->config.cpu.sbuf_len = MAX_SBUF_LEN - 1;
      WARNING ("sbuf_len too large; truncated.");
    }
  else if (val.int_val < 0)
    {
      sim->config.cpu.sbuf_len = 0;
      WARNING ("sbuf_len negative; disabled.");
    }
  else
    sim->config.cpu.sbuf_len = val.int_val;
}

/*---------------------------------------------------------------------------*/
/*!Register the functions to handle a section cpu

   This section does not allocate dynamically a data structure holding its
   config information. It's all in the global config.sim data
   structure. Therefore it does not need a start and end function to
   initialize default values (although it might be clearer to do so). The
   default values are set in init_defconfig().                               */
/*---------------------------------------------------------------------------*/
void
reg_cpu_sec (or1ksim *sim)
{
  struct config_section *sec = reg_config_sec (sim, "cpu", NULL, NULL);

  reg_config_param (sec, "ver",         paramt_int, cpu_ver);
  reg_config_param (sec, "cfg",         paramt_int, cpu_cfg);
  reg_config_param (sec, "rev",         paramt_int, cpu_rev);
  reg_config_param (sec, "upr",         paramt_int, cpu_upr);
  reg_config_param (sec, "cfgr",        paramt_int, cpu_cfgr);
  reg_config_param (sec, "sr",          paramt_int, cpu_sr);
  reg_config_param (sec, "superscalar", paramt_int, cpu_superscalar);
  reg_config_param (sec, "hazards",     paramt_int, cpu_hazards);
  reg_config_param (sec, "dependstats", paramt_int, cpu_dependstats);
  reg_config_param (sec, "sbuf_len",    paramt_int, cpu_sbuf_len);

}	/* reg_cpu_sec() */
