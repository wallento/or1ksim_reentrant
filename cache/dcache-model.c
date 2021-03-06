/* dcache-model.c -- data cache simulation

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

/* Cache functions.  At the moment these functions only simulate functionality
   of data caches and do not influence on fetche/decode/execute stages and
   timings.  They are here only to verify performance of various cache
   configurations. */


/* Autoconf and/or portability configuration */
#include "config.h"

/* Package includes */
#include "dcache-model.h"
#include "execute.h"
#include "spr-defs.h"
#include "abstract.h"
#include "stats.h"
#include "misc.h"

void
dc_info (or1ksim *sim)
{
  if (!(sim->cpu_state.sprs[SPR_UPR] & SPR_UPR_DCP))
    {
      PRINTF ("DCache not implemented. Set UPR[DCP].\n");
      return;
    }

  PRINTF ("Data cache %dKB: ",
	  sim->config.dc.nsets * sim->config.dc.blocksize * sim->config.dc.nways / 1024);
  PRINTF ("%d ways, %d sets, block size %d bytes\n", sim->config.dc.nways,
	  sim->config.dc.nsets, sim->config.dc.blocksize);
}

/* First check if data is already in the cache and if it is:
    - increment DC read hit stats,
    - set 'lru' at this way to config.dc.ustates - 1 and
      decrement 'lru' of other ways unless they have reached 0,
   and if not:
    - increment DC read miss stats
    - find lru way and entry and replace old tag with tag of the 'dataaddr'
    - set 'lru' with config.dc.ustates - 1 and decrement 'lru' of other
      ways unless they have reached 0
    - refill cache line
*/

uint32_t
dc_simulate_read (or1ksim *sim,oraddr_t dataaddr, oraddr_t virt_addr, int width)
{
  int set, way = -1;
  int i;
  oraddr_t tagaddr;
  uint32_t tmp = 0;

  if (!(sim->cpu_state.sprs[SPR_UPR] & SPR_UPR_DCP) ||
      !(sim->cpu_state.sprs[SPR_SR] & SPR_SR_DCE) || sim->data_ci)
    {
      if (width == 4)
	tmp = evalsim_mem32 (sim,dataaddr, virt_addr);
      else if (width == 2)
	tmp = evalsim_mem16 (sim,dataaddr, virt_addr);
      else if (width == 1)
	tmp = evalsim_mem8 (sim,dataaddr, virt_addr);

      if (sim->cur_area && sim->cur_area->log)
	fprintf (sim->cur_area->log, "[%" PRIxADDR "] -> read %08" PRIx32 "\n",
		 dataaddr, tmp);

      return tmp;
    }

  /* Which set to check out? */
  set = (dataaddr / sim->config.dc.blocksize) % sim->config.dc.nsets;
  tagaddr = (dataaddr / sim->config.dc.blocksize) / sim->config.dc.nsets;

  /* Scan all ways and try to find a matching way. */
  for (i = 0; i < sim->config.dc.nways; i++)
    if (sim->dc[set].way[i].tagaddr == tagaddr)
      way = i;

  /* Did we find our cached data? */
  if (way >= 0)
    {				/* Yes, we did. */
      sim->dc_stats.readhit++;

      for (i = 0; i < sim->config.dc.nways; i++)
	if (sim->dc[set].way[i].lru > sim->dc[set].way[way].lru)
	  sim->dc[set].way[i].lru--;
      sim->dc[set].way[way].lru = sim->config.dc.ustates - 1;
      sim->runtime.sim.mem_cycles += sim->config.dc.load_hitdelay;

      tmp =
	sim->dc[set].way[way].line[(dataaddr & (sim->config.dc.blocksize - 1)) >> 2];
      if (width == 4)
	return tmp;
      else if (width == 2)
	{
	  tmp = ((tmp >> ((dataaddr & 2) ? 0 : 16)) & 0xffff);
	  return tmp;
	}
      else if (width == 1)
	{
	  tmp = ((tmp >> (8 * (3 - (dataaddr & 3)))) & 0xff);
	  return tmp;
	}
    }
  else
    {				/* No, we didn't. */
      int minlru = sim->config.dc.ustates - 1;
      int minway = 0;

      sim->dc_stats.readmiss++;

      for (i = 0; i < sim->config.dc.nways; i++)
	{
	  if (sim->dc[set].way[i].lru < minlru)
	    {
	      minway = i;
	      minlru = sim->dc[set].way[i].lru;
	    }
	}

      for (i = 0; i < (sim->config.dc.blocksize); i += 4)
	{
	  /* FIXME: What is the virtual address meant to be? (ie. What happens if
	   * we read out of memory while refilling a cache line?) */
	  tmp =
	    evalsim_mem32 (sim,(dataaddr & ~(sim->config.dc.blocksize - 1)) +
			   (((dataaddr & ~ADDR_C (3)) +
			     i) & (sim->config.dc.blocksize - 1)), 0);

	  sim->dc[set].way[minway].
	    line[((dataaddr + i) & (sim->config.dc.blocksize - 1)) >> 2] = tmp;
	  if (!sim->cur_area)
	    {
	      sim->dc[set].way[minway].tagaddr = -1;
	      sim->dc[set].way[minway].lru = 0;
	      return 0;
	    }
	  else if (sim->cur_area->log)
	    fprintf (sim->cur_area->log, "[%" PRIxADDR "] -> read %08" PRIx32 "\n",
		     dataaddr, tmp);
	}

      sim->dc[set].way[minway].tagaddr = tagaddr;
      for (i = 0; i < sim->config.dc.nways; i++)
	if (sim->dc[set].way[i].lru)
	  sim->dc[set].way[i].lru--;
      sim->dc[set].way[minway].lru = sim->config.dc.ustates - 1;
      sim->runtime.sim.mem_cycles += sim->config.dc.load_missdelay;

      tmp =
	sim->dc[set].way[minway].line[(dataaddr & (sim->config.dc.blocksize - 1)) >> 2];
      if (width == 4)
	return tmp;
      else if (width == 2)
	{
	  tmp = (tmp >> ((dataaddr & 2) ? 0 : 16)) & 0xffff;
	  return tmp;
	}
      else if (width == 1)
	{
	  tmp = (tmp >> (8 * (3 - (dataaddr & 3)))) & 0xff;
	  return tmp;
	}
    }
  return 0;
}

/* First check if data is already in the cache and if it is:
    - increment DC write hit stats,
    - set 'lru' at this way to config.dc.ustates - 1 and
      decrement 'lru' of other ways unless they have reached 0,
   and if not:
    - increment DC write miss stats
    - find lru way and entry and replace old tag with tag of the 'dataaddr'
    - set 'lru' with config.dc.ustates - 1 and decrement 'lru' of other
      ways unless they have reached 0
*/

void
dc_simulate_write (or1ksim *sim,oraddr_t dataaddr, oraddr_t virt_addr, uint32_t data,
		   int width)
{
  int set, way = -1;
  int i;
  oraddr_t tagaddr;
  uint32_t tmp;

  if (width == 4)
    setsim_mem32 (sim, dataaddr, virt_addr, data);
  else if (width == 2)
    setsim_mem16 (sim, dataaddr, virt_addr, data);
  else if (width == 1)
    setsim_mem8 (sim, dataaddr, virt_addr, data);

  if (!(sim->cpu_state.sprs[SPR_UPR] & SPR_UPR_DCP) ||
      !(sim->cpu_state.sprs[SPR_SR] & SPR_SR_DCE) || sim->data_ci || !sim->cur_area)
    return;

  /* Which set to check out? */
  set = (dataaddr / sim->config.dc.blocksize) % sim->config.dc.nsets;
  tagaddr = (dataaddr / sim->config.dc.blocksize) / sim->config.dc.nsets;

  /* Scan all ways and try to find a matching way. */
  for (i = 0; i < sim->config.dc.nways; i++)
    if (sim->dc[set].way[i].tagaddr == tagaddr)
      way = i;

  /* Did we find our cached data? */
  if (way >= 0)
    {				/* Yes, we did. */
      sim->dc_stats.writehit++;

      for (i = 0; i < sim->config.dc.nways; i++)
	if (sim->dc[set].way[i].lru > sim->dc[set].way[way].lru)
	  sim->dc[set].way[i].lru--;
      sim->dc[set].way[way].lru = sim->config.dc.ustates - 1;
      sim->runtime.sim.mem_cycles += sim->config.dc.store_hitdelay;

      tmp =
	sim->dc[set].way[way].line[(dataaddr & (sim->config.dc.blocksize - 1)) >> 2];
      if (width == 4)
	tmp = data;
      else if (width == 2)
	{
	  tmp &= 0xffff << ((dataaddr & 2) ? 16 : 0);
	  tmp |= (data & 0xffff) << ((dataaddr & 2) ? 0 : 16);
	}
      else if (width == 1)
	{
	  tmp &= ~(0xff << (8 * (3 - (dataaddr & 3))));
	  tmp |= (data & 0xff) << (8 * (3 - (dataaddr & 3)));
	}
      sim->dc[set].way[way].line[(dataaddr & (sim->config.dc.blocksize - 1)) >> 2] =
	tmp;
    }
  else
    {				/* No, we didn't. */
      int minlru = sim->config.dc.ustates - 1;
      int minway = 0;

      sim->dc_stats.writemiss++;

      for (i = 0; i < sim->config.dc.nways; i++)
	if (sim->dc[set].way[i].lru < minlru)
	  minway = i;

      for (i = 0; i < (sim->config.dc.blocksize); i += 4)
	{
	  sim->dc[set].way[minway].
	    line[((dataaddr + i) & (sim->config.dc.blocksize - 1)) >> 2] =
	    /* FIXME: Same comment as in dc_simulate_read */
	    evalsim_mem32 (sim, (dataaddr & ~(sim->config.dc.blocksize - 1)) +
			   (((dataaddr & ~3ul) + i) & (sim->config.dc.blocksize -
						       1)), 0);
	  if (!sim->cur_area)
	    {
	      sim->dc[set].way[minway].tagaddr = -1;
	      sim->dc[set].way[minway].lru = 0;
	      return;
	    }
	}

      sim->dc[set].way[minway].tagaddr = tagaddr;
      for (i = 0; i < sim->config.dc.nways; i++)
	if (sim->dc[set].way[i].lru)
	  sim->dc[set].way[i].lru--;
      sim->dc[set].way[minway].lru = sim->config.dc.ustates - 1;
      sim->runtime.sim.mem_cycles += sim->config.dc.store_missdelay;
    }
}

/* First check if data is already in the cache and if it is:
    - invalidate block if way isn't locked
   otherwise don't do anything.
*/

void
dc_inv (or1ksim *sim,oraddr_t dataaddr)
{
  int set, way = -1;
  int i;
  oraddr_t tagaddr;

  if (!(sim->cpu_state.sprs[SPR_UPR] & SPR_UPR_DCP))
    return;

  /* Which set to check out? */
  set = (dataaddr / sim->config.dc.blocksize) % sim->config.dc.nsets;
  tagaddr = (dataaddr / sim->config.dc.blocksize) / sim->config.dc.nsets;

  if (!(sim->cpu_state.sprs[SPR_SR] & SPR_SR_DCE))
    {
      for (i = 0; i < sim->config.dc.nways; i++)
	{
	  sim->dc[set].way[i].tagaddr = -1;
	  sim->dc[set].way[i].lru = 0;
	}
      return;
    }
  /* Scan all ways and try to find a matching way. */
  for (i = 0; i < sim->config.dc.nways; i++)
    if (sim->dc[set].way[i].tagaddr == tagaddr)
      way = i;

  /* Did we find our cached data? */
  if (way >= 0)
    {				/* Yes, we did. */
      sim->dc[set].way[way].tagaddr = -1;
      sim->dc[set].way[way].lru = 0;
    }
}

/*-----------------------------------------------------[ DC configuration ]---*/

/*---------------------------------------------------------------------------*/
/*!Enable or disable the data cache

   Set the corresponding field in the UPR

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
dc_enabled (or1ksim *sim,union param_val  val,
	    void            *dat)
{
  if (val.int_val)
    {
      sim->cpu_state.sprs[SPR_UPR] |= SPR_UPR_DCP;
    }
  else
    {
      sim->cpu_state.sprs[SPR_UPR] &= ~SPR_UPR_DCP;
    }

  sim->config.dc.enabled = val.int_val;

    }	/* dc_enabled() */


/*---------------------------------------------------------------------------*/
/*!Set the number of data cache sets

   Value must be a power of 2 <= MAX_DC_SETS. If not issue a warning and
   ignore. Set the relevant field in the data cache config register

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
dc_nsets (or1ksim *sim,union param_val  val,
	  void            *dat)
{
  if (is_power2 (val.int_val) && (val.int_val <= MAX_DC_SETS))
    {
      int  set_bits = log2_int (val.int_val);

      sim->config.dc.nsets = val.int_val;

      sim->cpu_state.sprs[SPR_DCCFGR] &= ~SPR_DCCFGR_NCS;
      sim->cpu_state.sprs[SPR_DCCFGR] |= set_bits << SPR_DCCFGR_NCS_OFF;
    }
  else
    {
      fprintf (stderr, "Warning: data cache nsets not a power of 2 <= %d: "
	       "ignored\n", MAX_DC_SETS);
    }
}	/* dc_nsets() */


/*---------------------------------------------------------------------------*/
/*!Set the number of data cache ways

   Value must be a power of 2 <= MAX_DC_WAYS. If not issue a warning and
   ignore. Set the relevant field in the data cache config register

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
dc_nways (or1ksim *sim,union param_val  val,
	  void            *dat)
{
  if (is_power2 (val.int_val) && (val.int_val <= MAX_DC_WAYS))
    {
      int  way_bits = log2_int (val.int_val);

      sim->config.dc.nways = val.int_val;

      sim->cpu_state.sprs[SPR_DCCFGR] &= ~SPR_DCCFGR_NCW;
      sim->cpu_state.sprs[SPR_DCCFGR] |= way_bits << SPR_DCCFGR_NCW_OFF;
    }
  else
    {
      fprintf (stderr, "Warning: data cache nways not a power of 2 <= %d: "
	       "ignored\n", MAX_DC_WAYS);
    }
}	 /* dc_nways() */


/*---------------------------------------------------------------------------*/
/*!Set the data cache block size

   Value must be either MIN_DC_BLOCK_SIZE or MAX_DC_BLOCK_SIZE. If not issue a
   warning and ignore. Set the relevant field in the data cache config register

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
dc_blocksize (or1ksim *sim,union param_val  val,
	      void            *dat)
{
  switch (val.int_val)
    {
    case MIN_DC_BLOCK_SIZE:
      sim->config.dc.blocksize         = val.int_val;
      sim->cpu_state.sprs[SPR_DCCFGR] &= ~SPR_DCCFGR_CBS;
      break;

    case MAX_DC_BLOCK_SIZE:
      sim->config.dc.blocksize         = val.int_val;
      sim->cpu_state.sprs[SPR_DCCFGR] |= SPR_DCCFGR_CBS;
      break;

    default:
      fprintf (stderr, "Warning: data cache block size not %d or %d: "
	       "ignored\n", MIN_DC_BLOCK_SIZE, MAX_DC_BLOCK_SIZE);
      break;
    }
}	/* dc_blocksize() */


/*---------------------------------------------------------------------------*/
/*!Set the number of data cache usage states

   Value must be 2, 3 or 4. If not issue a warning and ignore.

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
dc_ustates (or1ksim *sim,union param_val  val,
	    void            *dat)
{
  if ((val.int_val >= 2) && (val.int_val <= 4))
    {
      sim->config.dc.ustates = val.int_val;
    }
  else
    {
      fprintf (stderr, "Warning number of data cache usage states must be "
	       "2, 3 or 4: ignored\n");
    }
}	/* dc_ustates() */


static void
dc_load_hitdelay (or1ksim *sim,union param_val val, void *dat)
{
  sim->config.dc.load_hitdelay = val.int_val;
}

static void
dc_load_missdelay (or1ksim *sim,union param_val val, void *dat)
{
  sim->config.dc.load_missdelay = val.int_val;
}

static void
dc_store_hitdelay (or1ksim *sim,union param_val val, void *dat)
{
  sim->config.dc.store_hitdelay = val.int_val;
}

static void
dc_store_missdelay (or1ksim *sim,union param_val val, void *dat)
{
  sim->config.dc.store_missdelay = val.int_val;
}

void
reg_dc_sec (or1ksim* sim)
{
  struct config_section *sec = reg_config_sec (sim, "dc", NULL, NULL);

  reg_config_param (sec, "enabled", paramt_int, dc_enabled);
  reg_config_param (sec, "nsets", paramt_int, dc_nsets);
  reg_config_param (sec, "nways", paramt_int, dc_nways);
  reg_config_param (sec, "blocksize", paramt_int, dc_blocksize);
  reg_config_param (sec, "ustates", paramt_int, dc_ustates);
  reg_config_param (sec, "load_hitdelay", paramt_int, dc_load_hitdelay);
  reg_config_param (sec, "load_missdelay", paramt_int, dc_load_missdelay);
  reg_config_param (sec, "store_hitdelay", paramt_int, dc_store_hitdelay);
  reg_config_param (sec, "store_missdelay", paramt_int, dc_store_missdelay);
}
