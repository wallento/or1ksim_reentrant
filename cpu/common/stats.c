/* stats.c -- Various statistics about instruction scheduling etc.

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


/* Autoconf and/or portability configuration */
#include "config.h"
#include "port.h"

/* Package includes */
#include "stats.h"
#include "sim-config.h"
#include "icache-model.h"
#include "spr-defs.h"
#include "execute.h"

/* Used by safe division - increment divisor by one if it is zero */
#define SD(X) (X != 0 ? X : 1)

struct branchstat
{
  int taken;
  int nottaken;
  int forward;
  int backward;
};

/*! @see also enum insn_type in abstract.h */
static const char func_unit_str[30][30] = {
  "unknown",
  "exception",
  "arith",
  "shift",
  "compare",
  "branch",
  "jump",
  "load",
  "store",
  "movimm",
  "move",
  "extend",
  "nop",
  "mac"
};

void
addsstats (or1ksim *sim, int item, int cnt_dynamic)
{
  int i = 0;

  while (sim->sstats[i].insn != item && sim->sstats[i].insn >= 0 && i < SSTATS_LEN)
    i++;

  if (i >= SSTATS_LEN - 1)
    return;

  if (sim->sstats[i].insn >= 0)
    {
      sim->sstats[i].cnt_dynamic += cnt_dynamic;
    }
  else
    {
      sim->sstats[i].insn = item;
      sim->sstats[i].cnt_dynamic = cnt_dynamic;
    }
}

void
adddstats (or1ksim *sim, int item1, int item2, int cnt_dynamic, int depend)
{
  int i = 0;

  while ((sim->dstats[i].insn1 != item1 || sim->dstats[i].insn2 != item2)
	 && (i < DSTATS_LEN) && sim->dstats[i].insn1 >= 0)
    i++;

  if (i >= DSTATS_LEN - 1)
    return;

  if (sim->dstats[i].insn1 >= 0)
    {
      sim->dstats[i].cnt_dynamic += cnt_dynamic;
      sim->dstats[i].depend += depend;
    }
  else
    {
      sim->dstats[i].insn1 = item1;
      sim->dstats[i].insn2 = item2;
      sim->dstats[i].cnt_dynamic = cnt_dynamic;
      sim->dstats[i].depend = depend;
    }
}

void
addfstats (or1ksim *sim, enum insn_type item1, enum insn_type item2, int cnt_dynamic,
	   int depend)
{
  int i = 0;

  while (((sim->fstats[i].insn1 != item1) || (sim->fstats[i].insn2 != item2)) &&
	 (sim->fstats[i].insn1 != it_unknown) && (i < FSTATS_LEN))
    i++;

  if (i >= FSTATS_LEN - 1)
    return;

  if ((sim->fstats[i].insn1 == item1) && (sim->fstats[i].insn2 == item2))
    {
      sim->fstats[i].cnt_dynamic += cnt_dynamic;
      sim->fstats[i].depend += depend;
    }
  else
    {
      sim->fstats[i].insn1 = item1;
      sim->fstats[i].insn2 = item2;
      sim->fstats[i].cnt_dynamic = cnt_dynamic;
      sim->fstats[i].depend = depend;
    }
}

void
initstats (or1ksim *sim)
{
  int i;
  memset (sim->sstats, 0, sizeof (sim->sstats));
  for (i = 0; i < SSTATS_LEN; i++)
	  sim->sstats[i].insn = -1;
  memset (sim->dstats, 0, sizeof (sim->dstats));
  for (i = 0; i < DSTATS_LEN; i++)
	  sim->dstats[i].insn1 = sim->dstats[i].insn2 = -1;
  memset (sim->fstats, 0, sizeof (sim->fstats));
  memset (&sim->or1k_mstats, 0, sizeof (sim->or1k_mstats));
  memset (&sim->ic_stats, 0, sizeof (sim->ic_stats));
  memset (&sim->dc_stats, 0, sizeof (sim->dc_stats));
  memset (&sim->raw_stats, 0, sizeof (sim->raw_stats));
}

static void
printotherstats (or1ksim *sim,int which)
{
  PRINTF ("\n");
  if (sim->config.bpb.enabled)
    {
      struct branchstat bf;
      struct branchstat bnf;
      long bf_all, bnf_all;
      bf.taken = sim->or1k_mstats.bf[1][0] + sim->or1k_mstats.bf[1][1];
      bf.nottaken = sim->or1k_mstats.bf[0][0] + sim->or1k_mstats.bf[0][1];
      bf.forward = sim->or1k_mstats.bf[0][1] + sim->or1k_mstats.bf[1][1];
      bf.backward = sim->or1k_mstats.bf[0][0] + sim->or1k_mstats.bf[1][0];
      bf_all = bf.forward + bf.backward;

      bnf.taken = sim->or1k_mstats.bnf[1][0] + sim->or1k_mstats.bf[1][1];
      bnf.nottaken = sim->or1k_mstats.bnf[0][0] + sim->or1k_mstats.bf[0][1];
      bnf.forward = sim->or1k_mstats.bnf[0][1] + sim->or1k_mstats.bf[1][1];
      bnf.backward = sim->or1k_mstats.bnf[0][0] + sim->or1k_mstats.bf[1][0];
      bnf_all = bnf.forward + bnf.backward;

      PRINTF ("bnf: %d (%ld%%) taken,", bf.taken,
	      (bf.taken * 100) / SD (bf_all));
      PRINTF (" %d (%ld%%) not taken,", bf.nottaken,
	      (bf.nottaken * 100) / SD (bf_all));
      PRINTF (" %d (%ld%%) forward,", bf.forward,
	      (bf.forward * 100) / SD (bf_all));
      PRINTF (" %d (%ld%%) backward\n", bf.backward,
	      (bf.backward * 100) / SD (bf_all));
      PRINTF ("bf: %d (%ld%%) taken,", bnf.taken,
	      (bnf.taken * 100) / SD (bnf_all));
      PRINTF (" %d (%ld%%) not taken,", bnf.nottaken,
	      (bnf.nottaken * 100) / SD (bnf_all));
      PRINTF (" %d (%ld%%) forward,", bnf.forward,
	      (bnf.forward * 100) / SD (bnf_all));
      PRINTF (" %d (%ld%%) backward\n", bnf.backward,
	      (bnf.backward * 100) / SD (bnf_all));

      PRINTF ("StaticBP bnf(%s): correct %ld%%\n",
	      sim->config.bpb.sbp_bnf_fwd ? "forward" : "backward",
	      (sim->or1k_mstats.bnf[0][sim->config.bpb.sbp_bnf_fwd] * 100) /
	      SD (bnf_all));
      PRINTF ("StaticBP bf(%s): correct %ld%%\n",
	      sim->config.bpb.sbp_bf_fwd ? "forward" : "backward",
	      (sim->or1k_mstats.bnf[1][sim->config.bpb.sbp_bf_fwd] * 100) /
	      SD (bf_all));
      PRINTF ("BPB: hit %d (correct %d%%), miss %d\n", sim->or1k_mstats.bpb.hit,
	      (sim->or1k_mstats.bpb.correct * 100) / SD (sim->or1k_mstats.bpb.hit),
	      sim->or1k_mstats.bpb.miss);
    }
  else
    PRINTF ("BPB simulation disabled. Enable it to see BPB analysis\n");

  if (sim->config.bpb.btic)
    {
      PRINTF ("BTIC: hit %d(%d%%), miss %d\n", sim->or1k_mstats.btic.hit,
	      (sim->or1k_mstats.btic.hit * 100) / SD (sim->or1k_mstats.btic.hit +
						 sim->or1k_mstats.btic.miss),
	      sim->or1k_mstats.btic.miss);
    }
  else
    PRINTF ("BTIC simulation disabled. Enabled it to see BTIC analysis\n");

  if ((NULL != sim->ic_state) && sim->ic_state->enabled)
    {
      PRINTF ("IC read:  hit %d(%d%%), miss %d\n", sim->ic_stats.readhit,
	      (sim->ic_stats.readhit * 100) / SD (sim->ic_stats.readhit +
					     sim->ic_stats.readmiss),
	      sim->ic_stats.readmiss);
    }
  else
    PRINTF ("No ICache. Enable it to see IC results.\n");

  if (sim->config.dc.enabled)
    {
      PRINTF ("DC read:  hit %d(%d%%), miss %d\n", sim->dc_stats.readhit,
	      (sim->dc_stats.readhit * 100) / SD (sim->dc_stats.readhit +
					     sim->dc_stats.readmiss),
	      sim->dc_stats.readmiss);
      PRINTF ("DC write: hit %d(%d%%), miss %d\n", sim->dc_stats.writehit,
	      (sim->dc_stats.writehit * 100) / SD (sim->dc_stats.writehit +
					      sim->dc_stats.writemiss),
	      sim->dc_stats.writemiss);
    }
  else
    PRINTF ("No DCache. Enable it to see DC results.\n");

  if (sim->cpu_state.sprs[SPR_UPR] & SPR_UPR_IMP)
    {
      PRINTF ("IMMU read:  hit %d(%d%%), miss %d\n", sim->immu_stats.fetch_tlbhit,
	      (sim->immu_stats.fetch_tlbhit * 100) / SD (sim->immu_stats.fetch_tlbhit +
						    sim->immu_stats.fetch_tlbmiss),
	      sim->immu_stats.fetch_tlbmiss);
    }
  else
    PRINTF ("No IMMU. Set UPR[IMP]\n");

  if (sim->cpu_state.sprs[SPR_UPR] & SPR_UPR_DMP)
    {
      PRINTF ("DMMU read:  hit %d(%d%%), miss %d\n", sim->dmmu_stats.loads_tlbhit,
	      (sim->dmmu_stats.loads_tlbhit * 100) / SD (sim->dmmu_stats.loads_tlbhit +
						    sim->dmmu_stats.loads_tlbmiss),
	      sim->dmmu_stats.loads_tlbmiss);
    }
  else
    PRINTF ("No DMMU. Set UPR[DMP]\n");

  PRINTF ("Additional LOAD CYCLES: %u  STORE CYCLES: %u\n",
	  sim->runtime.sim.loadcycles, sim->runtime.sim.storecycles);
}

void
printstats (or1ksim *sim,int which)
{
  int i, all = 0, dependall = 0;

  if (which > 1 && which <= 5 && !sim->config.cpu.dependstats)
    {
      PRINTF
	("Hazard analysis disabled. Enable it to see analysis results.\n");
      return;
    }

  switch (which)
    {
    case 1:
      PRINTF ("stats 1: Misc stats\n");
      printotherstats (sim,which);
      break;
    case 2:
      PRINTF ("stats 2: Instruction usage\n");
      for (i = 0; i < SSTATS_LEN; i++)
	all += sim->sstats[i].cnt_dynamic;

      for (i = 0; i < SSTATS_LEN; i++)
	if (sim->sstats[i].cnt_dynamic)
	  PRINTF ("  %-15s used %6dx (%5.1f%%)\n", insn_name (sim->sstats[i].insn),
		  sim->sstats[i].cnt_dynamic,
		  (sim->sstats[i].cnt_dynamic * 100.) / SD (all));

      PRINTF ("%d instructions (dynamic, single stats)\n", all);
      break;

    case 3:
      PRINTF ("stats 3: Instruction dependencies\n");
      for (i = 0; i < DSTATS_LEN; i++)
	{
	  all += sim->dstats[i].cnt_dynamic;
	  dependall += sim->dstats[i].depend;
	}

      for (i = 0; i < DSTATS_LEN; i++)
	if (sim->dstats[i].cnt_dynamic)
	  {
	    char temp[100];
	    sprintf (temp, "%s, %s ", insn_name (sim->dstats[i].insn1),
		     insn_name (sim->dstats[i].insn2));
	    PRINTF ("  %-30s %6dx (%5.1f%%)", temp, sim->dstats[i].cnt_dynamic,
		    (sim->dstats[i].cnt_dynamic * 100.) / SD (all));
	    PRINTF ("   depend: %5.1f%%\n",
		    (sim->dstats[i].depend * 100.) / sim->dstats[i].cnt_dynamic);
	  }

      PRINTF ("%d instructions (dynamic, dependency stats)  depend: %d%%\n",
	      all, (dependall * 100) / SD (all));
      break;

    case 4:
      PRINTF ("stats 4: Functional units dependencies\n");
      for (i = 0; i < FSTATS_LEN; i++)
	{
	  all += sim->fstats[i].cnt_dynamic;
	  dependall += sim->fstats[i].depend;
	}

      for (i = 0; i < FSTATS_LEN; i++)
	if (sim->fstats[i].cnt_dynamic)
	  {
	    char temp[100];
	    sprintf (temp, "%s, %s", func_unit_str[sim->fstats[i].insn1],
		     func_unit_str[sim->fstats[i].insn2]);
	    PRINTF ("  %-30s %6dx (%5.1f%%)", temp, sim->fstats[i].cnt_dynamic,
		    (sim->fstats[i].cnt_dynamic * 100.) / SD (all));
	    PRINTF ("   depend: %5.1f%%\n",
		    (sim->fstats[i].depend * 100.) / sim->fstats[i].cnt_dynamic);
	  }
      PRINTF
	("%d instructions (dynamic, functional units stats)  depend: %d%%\n\n",
	 all, (dependall * 100) / SD (all));
      break;

    case 5:
      PRINTF ("stats 5: Raw register usage over time\n");
#if RAW_RANGE_STATS
      for (i = 0; (i < RAW_RANGE); i++)
	PRINTF ("  Register set and reused in %d. cycle: %d cases\n", i,
		raw_stats.range[i]);
#endif
      break;
    case 6:
      if (sim->config.cpu.sbuf_len)
	{
	  extern int sbuf_total_cyc, sbuf_wait_cyc;
	  PRINTF ("stats 6: Store buffer analysis\n");
	  PRINTF ("Using store buffer of length %i.\n", sim->config.cpu.sbuf_len);
	  PRINTF ("Number of total memory store cycles: %i/%lli\n",
		  sim->sbuf_total_cyc,
		  sim->runtime.sim.cycles + sim->sbuf_total_cyc - sim->sbuf_wait_cyc);
	  PRINTF ("Number of cycles waiting for memory stores: %i\n",
		  sim->sbuf_wait_cyc);
	  PRINTF ("Number of memory cycles spared: %i\n",
		  sim->sbuf_total_cyc - sim->sbuf_wait_cyc);
	  PRINTF ("Store speedup %3.2f%%, total speedup %3.2f%%\n",
		  100. * (sim->sbuf_total_cyc - sim->sbuf_wait_cyc) / sim->sbuf_total_cyc,
		  100. * (sim->sbuf_total_cyc -
			  sim->sbuf_wait_cyc) / (sim->runtime.sim.cycles +
					    sim->sbuf_total_cyc - sim->sbuf_wait_cyc));
	}
      else
	PRINTF
	  ("Store buffer analysis disabled. Enable it to see analysis results.\n");
      break;
    default:
      PRINTF ("Please specify a stats group (1-6).\n");
      break;
    }
}
