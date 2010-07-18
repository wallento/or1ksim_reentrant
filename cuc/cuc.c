/* cuc.c -- OpenRISC Custom Unit Compiler

   Copyright (C) 2002 Marko Mlinar, markom@opencores.org
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

/* Main file, including code optimization and command prompt */


/* Autoconf and/or portability configuration */
#include "config.h"
#include "port.h"

/* System includes */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

/* Package includes */
#include "cuc.h"
#include "sim-config.h"
#include "profiler.h"
#include "insn.h"
#include "opcode/or32.h"
#include "parse.h"
#include "verilog.h"
#include "debug.h"
#include "siminstance.h"

/* Last used registers by software convention */
/* Note that r11 is caller saved register, and we can destroy it.
   Due to CUC architecture we must always return something, even garbage (so that
   caller knows, we are finished, when we send acknowledge).
   In case r11 was not used (trivial register assignment) we will remove it later,
   but if we assigned a value to it, it must not be removed, so caller_saved[11] = 0 */
const int caller_saved[MAX_REGS] = {
  0, 0, 0, 1, 1, 1, 1, 1,
  1, 1, 0, 0, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1, 0, 1,
  1, 1
};

/* Does all known instruction optimizations */
void
cuc_optimize (or1ksim *sim, cuc_func * func)
{
  int modified = 0;
  int first = 1;
  log ("Optimizing.\n");
  do
    {
      modified = 0;
      clean_deps (func);
      if (sim->cuc_debug >= 6)
	print_cuc_bb (sim, func, "AFTER_CLEAN_DEPS");
      if (optimize_cmovs (sim, func))
	{
	  if (sim->cuc_debug >= 6)
	    print_cuc_bb (sim, func, "AFTER_OPT_CMOVS");
	  modified = 1;
	}
      if (sim->cuc_debug)
	cuc_check (sim, func);
      if (optimize_tree (sim, func))
	{
	  if (sim->cuc_debug >= 6)
	    print_cuc_bb (sim, func, "AFTER_OPT_TREE1");
	  modified = 1;
	}
      if (remove_nops (sim, func))
	{
	  if (sim->cuc_debug >= 6)
	    print_cuc_bb (sim, func, "NO_NOPS");
	  modified = 1;
	}
      if (sim->cuc_debug)
	cuc_check (sim, func);
      if (remove_dead (sim, func))
	{
	  if (sim->cuc_debug >= 5)
	    print_cuc_bb (sim, func, "AFTER_DEAD");
	  modified = 1;
	}
      if (sim->cuc_debug)
	cuc_check (sim, func);
      if (cse (sim, func))
	{
	  log ("Common subexpression elimination.\n");
	  if (sim->cuc_debug >= 3)
	    print_cuc_bb (sim, func, "AFTER_CSE");
	  modified = 1;
	}
      if (first)
	{
	  insert_conditional_facts (sim, func);
	  if (sim->cuc_debug >= 3)
	    print_cuc_bb (sim, func, "AFTER_COND_FACT");
	  if (sim->cuc_debug)
	    cuc_check (sim, func);
	  first = 0;
	}
      if (optimize_bb (sim, func))
	{
	  if (sim->cuc_debug >= 5)
	    print_cuc_bb (sim, func, "AFTER_OPT_BB");
	  modified = 1;
	}
      if (sim->cuc_debug)
	cuc_check (sim, func);
      if (remove_nops (sim, func))
	{
	  if (sim->cuc_debug >= 6)
	    print_cuc_bb (sim, func, "NO_NOPS");
	  modified = 1;
	}
      if (remove_dead_bb (sim, func))
	{
	  if (sim->cuc_debug >= 5)
	    print_cuc_bb (sim, func, "AFTER_DEAD_BB");
	  modified = 1;
	}
      if (remove_trivial_regs (sim, func))
	{
	  if (sim->cuc_debug >= 2)
	    print_cuc_bb (sim, func, "AFTER_TRIVIAL");
	  modified = 1;
	}
      if (remove_nops (sim, func))
	{
	  if (sim->cuc_debug >= 6)
	    print_cuc_bb (sim, func, "NO_NOPS");
	  modified = 1;
	}
      add_memory_dep (func, func->memory_order);
      if (sim->cuc_debug >= 7)
	print_cuc_bb (sim, func, "AFTER_MEMORY_DEP");
      add_data_dep (func);
      if (sim->cuc_debug >= 8)
	print_cuc_bb (sim, func, "AFTER_DATA_DEP");
      if (schedule_memory (sim, func, func->memory_order))
	{
	  if (sim->cuc_debug >= 7)
	    print_cuc_bb (sim, func, "AFTER_SCHEDULE_MEM");
	  modified = 1;
	}
    }
  while (modified);
  set_io (sim, func);
#if 0
  detect_max_values (func);
  if (cuc_debug >= 5)
    print_cuc_bb (func, "AFTER_MAX_VALUES");
#endif
}

/* Pre/unrolls basic block and optimizes it */
cuc_timings *
preunroll_bb (or1ksim *sim, char *bb_filename, cuc_func * f, cuc_timings * timings, int b,
	      int i, int j)
{
  cuc_func *func;
  cucdebug (2, "BB%i unroll %i times preroll %i times\n", b, j, i);
  log ("BB%i unroll %i times preroll %i times\n", b, j, i);
  func = preunroll_loop (sim, f, b, i, j, bb_filename);
  if (sim->cuc_debug >= 2)
    print_cuc_bb (sim, func, "AFTER_PREUNROLL");
  cuc_optimize (sim, func);
  analyse_timings (sim, func, timings);

  cucdebug (2, "new_time = %i, old_time = %i, size = %f\n",
	    timings->new_time, func->orig_time, timings->size);
  log ("new time = %icyc, old_time = %icyc, size = %.0f gates\n",
       timings->new_time, func->orig_time, timings->size);
  //output_verilog (func, argv[1]);
  free_func (func);
  timings->b = b;
  timings->unroll = j;
  timings->preroll = i;
  timings->nshared = 0;
  return timings;
}


/* Simple comparison function */
int
tim_comp (cuc_timings * a, cuc_timings * b)
{
  if (a->new_time < b->new_time)
    return -1;
  else if (a->new_time > b->new_time)
    return 1;
  else
    return 0;
}

/* Analyses function; done when cuc command is entered in (sim) prompt */
cuc_func *
analyse_function (or1ksim *sim, char *module_name, long orig_time,
		  unsigned long start_addr, unsigned long end_addr,
		  int memory_order, int num_runs)
{
  cuc_timings timings;
  cuc_func *func = (cuc_func *) malloc (sizeof (cuc_func));
  cuc_func *saved;
  int b, i, j;
  char tmp1[256];
  char tmp2[256];

  func->orig_time = orig_time;
  func->start_addr = start_addr;
  func->end_addr = end_addr;
  func->memory_order = memory_order;
  func->nfdeps = 0;
  func->fdeps = NULL;
  func->num_runs = num_runs;

  sprintf (tmp1, "%s.bin", module_name);
  cucdebug (2, "Loading %s.bin\n", module_name);
  if (cuc_load (sim, tmp1))
    {
      free (func);
      return NULL;
    }

  log ("Detecting basic blocks\n");
  detect_bb (sim, func);
  if (sim->cuc_debug >= 2)
    print_cuc_insns (sim, "WITH_BB_LIMITS", 0);

  //sprintf (tmp1, "%s.bin.mp", module_name);
  sprintf (tmp2, "%s.bin.bb", module_name);
  generate_bb_seq (sim, func, sim->config.sim.mprof_fn, tmp2);
  log ("Assuming %i clk cycle load (%i cyc burst)\n", sim->runtime.cuc.mdelay[0],
       sim->runtime.cuc.mdelay[2]);
  log ("Assuming %i clk cycle store (%i cyc burst)\n", sim->runtime.cuc.mdelay[1],
       sim->runtime.cuc.mdelay[3]);

  build_bb (sim, func);
  if (sim->cuc_debug >= 5)
    print_cuc_bb (sim, func, "AFTER_BUILD_BB");
  reg_dep (sim, func);

  log ("Detecting dependencies\n");
  if (sim->cuc_debug >= 2)
    print_cuc_bb (sim, func, "AFTER_REG_DEP");
  cuc_optimize (sim, func);

#if 0
  csm (func);
#endif
  assert (saved = dup_func (func));

  timings.preroll = timings.unroll = 1;
  timings.nshared = 0;

  add_latches (sim, func);
  if (sim->cuc_debug >= 1)
    print_cuc_bb (sim, func, "AFTER_LATCHES");
  analyse_timings (sim, func, &timings);

  free_func (func);
  log ("Base option: pre%i,un%i,sha%i: %icyc %.1f\n",
       timings.preroll, timings.unroll, timings.nshared, timings.new_time,
       timings.size);
  saved->timings = timings;

#if 1
  /* detect and unroll simple loops */
  for (b = 0; b < saved->num_bb; b++)
    {
      cuc_timings t[MAX_UNROLL * MAX_PREROLL];
      cuc_timings *ut;
      cuc_timings *cut = &t[0];
      int nt = 1;
      double csize;
      saved->bb[b].selected_tim = -1;

      /* Is it a loop? */
      if (saved->bb[b].next[0] != b && saved->bb[b].next[1] != b)
	continue;
      log ("Found loop at BB%x.  Trying to unroll.\n", b);
      t[0] = timings;
      t[0].b = b;
      t[0].preroll = 1;
      t[0].unroll = 1;
      t[0].nshared = 0;

      sprintf (tmp1, "%s.bin.bb", module_name);
      i = 1;
      do
	{
	  cuc_timings *pt;
	  cuc_timings *cpt = cut;
	  j = 1;

	  do
	    {
	      pt = cpt;
	      cpt = preunroll_bb (sim, tmp1, saved, &t[nt++], b, ++j, i);
	    }
	  while (j <= MAX_PREROLL && pt->new_time > cpt->new_time);
	  i++;
	  ut = cut;
	  cut = preunroll_bb (sim, tmp1, saved, &t[nt++], b, 1, i);
	}
      while (i <= MAX_UNROLL && ut->new_time > cut->new_time);

      /* Sort the timings */
#if 0
      if (cuc_debug >= 3)
	for (i = 0; i < nt; i++)
	  PRINTF ("%i:%i,%i: %icyc\n",
		  t[i].b, t[i].preroll, t[i].unroll, t[i].new_time);
#endif

#if HAVE___COMPAR_FN_T
      qsort (t, nt, sizeof (cuc_timings),
	     (__compar_fn_t) tim_comp);
#else
      qsort (t, nt, sizeof (cuc_timings),
	     (int (*) (const void *, const void *)) tim_comp);
#endif

      /* Delete timings, that have worst time and bigger size than other */
      j = 1;
      csize = t[0].size;
      for (i = 1; i < nt; i++)
	if (t[i].size < csize)
	  t[j++] = t[i];
      nt = j;

      cucdebug (1, "Available options\n");
      for (i = 0; i < nt; i++)
	cucdebug (1, "%i:%i,%i: %icyc %.1f\n",
		  t[i].b, t[i].preroll, t[i].unroll, t[i].new_time,
		  t[i].size);
      /* Add results from CSM */
      j = nt;
      for (i = 0; i < saved->bb[b].ntim; i++)
	{
	  int i1;
	  for (i1 = 0; i1 < nt; i1++)
	    {
	      t[j] = t[i1];
	      t[j].size += saved->bb[b].tim[i].size - timings.size;
	      t[j].new_time +=
		saved->bb[b].tim[i].new_time - timings.new_time;
	      t[j].nshared = saved->bb[b].tim[i].nshared;
	      t[j].shared = saved->bb[b].tim[i].shared;
	      if (++j >= MAX_UNROLL * MAX_PREROLL)
		goto full;
	    }
	}

    full:
      nt = j;

      cucdebug (1, "Available options:\n");
      for (i = 0; i < nt; i++)
	cucdebug (1, "%i:%i,%i: %icyc %.1f\n",
		  t[i].b, t[i].preroll, t[i].unroll, t[i].new_time,
		  t[i].size);

      /* Sort again with new timings added */
#if HAVE___COMPAR_FN_T
      qsort (t, nt, sizeof (cuc_timings),
	     (__compar_fn_t) tim_comp);
#else
      qsort (t, nt, sizeof (cuc_timings),
	     (int (*)(const void *, const void *)) tim_comp);
#endif

      /* Delete timings, that have worst time and bigger size than other */
      j = 1;
      csize = t[0].size;
      for (i = 1; i < nt; i++)
	if (t[i].size < csize)
	  t[j++] = t[i];
      nt = j;

      cucdebug (1, "Available options:\n");
      for (i = 0; i < nt; i++)
	cucdebug (1, "%i:%i,%i: %icyc %.1f\n",
		  t[i].b, t[i].preroll, t[i].unroll, t[i].new_time,
		  t[i].size);

      if (saved->bb[b].ntim)
	free (saved->bb[b].tim);
      saved->bb[b].ntim = nt;
      assert (saved->bb[b].tim =
	      (cuc_timings *) malloc (sizeof (cuc_timings) * nt));

      /* Copy options in reverse order -- smallest first */
      for (i = 0; i < nt; i++)
	saved->bb[b].tim[i] = t[nt - 1 - i];

      log ("Available options:\n");
      for (i = 0; i < saved->bb[b].ntim; i++)
	{
	  log ("%i:pre%i,un%i,sha%i: %icyc %.1f\n",
	       saved->bb[b].tim[i].b, saved->bb[b].tim[i].preroll,
	       saved->bb[b].tim[i].unroll, saved->bb[b].tim[i].nshared,
	       saved->bb[b].tim[i].new_time, saved->bb[b].tim[i].size);
	}
    }
#endif
  return saved;
}

/* Utility option formatting functions */
static const char *option_char =
  "?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

/*static */ char *
gen_option (char *s, int bb_no, int f_opt)
{
  if (bb_no >= 0)
    sprintf (s, "%i", bb_no);
  assert (f_opt <= strlen (option_char));
  sprintf (s, "%s%c", s, option_char[f_opt]);
  return s;
}

/*static */ void
print_option (or1ksim *sim, int bb_no, int f_opt)
{
  char tmp1[10];
  char tmp2[10];
  sprintf (tmp2, "%s", gen_option (tmp1, bb_no, f_opt));
  PRINTF ("%3s", tmp2);
}

static char *
format_func_options (char *s, cuc_func * f)
{
  int b, first = 1;
  *s = '\0';
  for (b = 0; b < f->num_bb; b++)
    if (f->bb[b].selected_tim >= 0)
      {
	char tmp[10];
	sprintf (s, "%s%s%s", s, first ? "" : ",",
		 gen_option (tmp, b, f->bb[b].selected_tim));
	first = 0;
      }
  return s;
}

static void
options_cmd (or1ksim *sim, int func_no, cuc_func * f)
{
  int b, i;
  char tmp[30];
  char *name = sim->profiler.prof_func[func_no].name;
  PRINTF
    ("-----------------------------------------------------------------------------\n");
  PRINTF ("|%-28s|pre/unrolled|shared|  time  |  gates |old_time|\n",
	  strstrip (tmp, name, 28));
  PRINTF ("|                    BASE    |%4i / %4i | %4i |%8i|%8.f|%8i|\n", 1,
	  1, 0, f->timings.new_time, f->timings.size, f->orig_time);
  for (b = 0; b < f->num_bb; b++)
    {
      /* Print out results */
      for (i = 1; i < f->bb[b].ntim; i++)
	{			/* First one is base option */
	  int time = f->bb[b].tim[i].new_time - f->timings.new_time;
	  double size = f->bb[b].tim[i].size - f->timings.size;
	  PRINTF ("|                   ");
	  print_option (sim, b, i);
	  PRINTF ("      |%4i / %4i | %4i |%+8i|%+8.f|        |\n",
		  f->bb[b].tim[i].preroll, f->bb[b].tim[i].unroll,
		  f->bb[b].tim[i].nshared, time, size);
	}
    }
}

/* Generates a function, based on specified parameters */
cuc_func *
generate_function (or1ksim *sim, cuc_func * rf, char *name, char *cut_filename)
{
  int b;
  char tmp[256];
  cuc_timings tt;
  cuc_func *f;
  assert (f = dup_func (rf));

  if (sim->cuc_debug >= 2)
    print_cuc_bb (sim, f, "BEFORE_GENERATE");
  log ("Generating function %s.\n", name);
  PRINTF ("Generating function %s.\n", name);

  format_func_options (tmp, rf);
  if (strlen (tmp))
    PRINTF ("Applying options: %s\n", tmp);
  else
    PRINTF ("Using basic options.\n");

  /* Generate function as specified by options */
  for (b = 0; b < f->num_bb; b++)
    {
      cuc_timings *st;
      if (rf->bb[b].selected_tim < 0)
	continue;
      st = &rf->bb[b].tim[rf->bb[b].selected_tim];
      sprintf (tmp, "%s.bin.bb", name);
      preunroll_bb (sim, &tmp[0], f, &tt, b, st->preroll, st->unroll);
      if (sim->cuc_debug >= 1)
	print_cuc_bb (sim, f, "AFTER_PREUNROLL");
    }
  for (b = 0; b < f->num_bb; b++)
    {
      cuc_timings *st;
      if (rf->bb[b].selected_tim < 0)
	continue;
      st = &rf->bb[b].tim[rf->bb[b].selected_tim];
      if (!st->nshared)
	continue;
      assert (0);
      //csm_gen (f, rf, st->nshared, st->shared);
    }
  add_latches (sim, f);
  if (sim->cuc_debug >= 1)
    print_cuc_bb (sim, f, "AFTER_LATCHES");
  analyse_timings (sim, f, &tt);

  sprintf (tmp, "%s%s", cut_filename, name);
  output_verilog (f, tmp, name);
  return f;
}

/* Calculates required time, based on selected options */
int
calc_cycles (cuc_func * f)
{
  int b, ntime = f->timings.new_time;
  for (b = 0; b < f->num_bb; b++)
    if (f->bb[b].selected_tim >= 0)
      {
	assert (f->bb[b].selected_tim < f->bb[b].ntim);
	ntime +=
	  f->bb[b].tim[f->bb[b].selected_tim].new_time - f->timings.new_time;
      }
  return ntime;
}

/* Calculates required size, based on selected options */
double
calc_size (cuc_func * f)
{
  int b;
  double size = f->timings.size;
  for (b = 0; b < f->num_bb; b++)
    if (f->bb[b].selected_tim >= 0)
      {
	assert (f->bb[b].selected_tim < f->bb[b].ntim);
	size += f->bb[b].tim[f->bb[b].selected_tim].size - f->timings.size;
      }
  return size;
}

/* Dumps specified function to file (hex) */
unsigned long
extract_function (or1ksim *sim, char *out_fn, unsigned long start_addr)
{
  FILE *fo;
  unsigned long a = start_addr;
  int x = 0;
  assert (fo = fopen (out_fn, "wt+"));

  do
    {
      unsigned long d = eval_direct32 (sim, a, 0, 0);
      int index = insn_decode (sim, d);
      assert (index >= 0);
      if (x)
	x++;
      if (strcmp (insn_name (index), "l.jr") == 0)
	x = 1;
      a += 4;
      fprintf (fo, "%08lx\n", d);
    }
  while (x < 2);

  fclose (fo);
  return a - 4;
}

/* Detects function dependencies and removes  */
static void
set_func_deps (or1ksim *sim)
{
  int f, b, i, j;
restart:
  for (f = 0; f < sim->profiler.prof_nfuncs - 1; f++)
    if (sim->func[f])
      {
	int fused[MAX_FUNCS] = { 0 };
	int c = 0;
	for (b = 0; b < sim->func[f]->num_bb; b++)
	  for (i = 0; i < sim->func[f]->bb[b].ninsn; i++)
	    {
	      cuc_insn *ii = &(sim->func[f]->bb[b].insn[i]);
	      if (ii->index == II_CALL)
		{
		  assert (ii->opt[0] == OPT_CONST);
		  for (j = 0; j < sim->profiler.prof_nfuncs - 1; j++)
		    if (sim->func[j] && sim->func[j]->start_addr == ii->op[0])
		      break;
		  if (j >= sim->profiler.prof_nfuncs - 1)
		    {
		      log ("%s is calling unknown function, address %08lx\n",
			   sim->profiler.prof_func[f].name, ii->op[0]);
		      debug (sim,1,
			     "%s is calling unknown function, address %08lx\n",
			     sim->profiler.prof_func[f].name, ii->op[0]);
		      free_func (sim->func[f]);
		      sim->func[f] = NULL;
		      goto restart;
		    }
		  else if (f == j)
		    {
		      log ("%s is recursive, ignoring\n", sim->profiler.prof_func[f].name);
		      debug (sim,1, "%s is recursive, ignoring\n",
			     sim->profiler.prof_func[f].name);
		      free_func (sim->func[f]);
		      sim->func[f] = NULL;
		      goto restart;
		    }
		  else
		    fused[j]++;
		}
	    }
	for (i = 0; i < MAX_FUNCS; i++)
	  if (fused[i])
	    c++;
	if (sim->func[f]->nfdeps)
	  free (sim->func[f]->fdeps);
	sim->func[f]->nfdeps = c;
	sim->func[f]->fdeps = (cuc_func **) malloc (sizeof (cuc_func *) * c);
	for (i = 0, j = 0; i < MAX_FUNCS; i++)
	  if (fused[i])
	    sim->func[f]->fdeps[j++] = sim->func[i];
      }

  /* Detect loops */
  {
    int change;
    for (f = 0; f < MAX_FUNCS; f++)
      if (sim->func[f])
	sim->func[f]->tmp = 0;
    do
      {
	change = 0;
	for (f = 0; f < MAX_FUNCS; f++)
	  if (sim->func[f] && !sim->func[f]->tmp)
	    {
	      int o = 1;
	      for (i = 0; i < sim->func[f]->nfdeps; i++)
		if (!sim->func[f]->fdeps[i]->tmp)
		  {
		    o = 0;
		    break;
		  }
	      if (o)
		{
		  sim->func[f]->tmp = 1;
		  change = 1;
		}
	    }
      }
    while (change);

    change = 0;
    for (f = 0; f < MAX_FUNCS; f++)
      if (sim->func[f] && !sim->func[f]->tmp)
	{
	  free_func (sim->func[f]);
	  sim->func[f] = NULL;
	  change = 1;
	}
    if (change)
      goto restart;
  }
}

void
main_cuc (or1ksim *sim, char *filename)
{
  int i, j;
  char tmp1[256];
  char filename_cut[256];
#if 0				/* Select prefix, based on binary program name */
  for (i = 0; i < sizeof (filename_cut); i++)
    {
      if (isalpha (filename[i]))
	filename_cut[i] = filename[i];
      else
	{
	  filename_cut[i] = '\0';
	  break;
	}
    }
#else
  strcpy (filename_cut, "cu");
#endif

  PRINTF ("Entering OpenRISC Custom Unit Compiler command prompt\n");
  PRINTF ("Using profile file \"%s\" and memory profile file \"%s\".\n",
	  sim->config.sim.prof_fn, sim->config.sim.mprof_fn);
  sprintf (tmp1, "%s.log", filename_cut);
  PRINTF ("Analyzing. (log file \"%s\").\n", tmp1);
  assert (sim->flog = fopen (tmp1, "wt+"));

  /* Loads in the specified timings table */
  PRINTF ("Using timings from \"%s\" at %s\n", sim->config.cuc.timings_fn,
	  generate_time_pretty (tmp1, sim->config.sim.clkcycle_ps));
  load_timing_table (sim, sim->config.cuc.timings_fn);
  sim->runtime.cuc.cycle_duration = 1000. * sim->config.sim.clkcycle_ps;
  PRINTF ("Multicycle logic %s, bursts %s, %s memory order.\n",
	  sim->config.cuc.no_multicycle ? "OFF" : "ON",
	  sim->config.cuc.enable_bursts ? "ON" : "OFF",
	  sim->config.cuc.memory_order ==
	  MO_NONE ? "no" : sim->config.cuc.memory_order ==
	  MO_WEAK ? "weak" : sim->config.cuc.memory_order ==
	  MO_STRONG ? "strong" : "exact");

  prof_set (sim, 1, 0);
  assert (prof_acquire (sim, sim->config.sim.prof_fn) == 0);

  if (sim->config.cuc.calling_convention)
    PRINTF ("Assuming OpenRISC standard calling convention.\n");

  /* Try all functions except "total" */
  for (i = 0; i < sim->profiler.prof_nfuncs - 1; i++)
    {
      long orig_time;
      unsigned long start_addr, end_addr;
      orig_time = sim->profiler.prof_func[i].cum_cycles;
      start_addr = sim->profiler.prof_func[i].addr;

      /* Extract the function from the binary */
      sprintf (tmp1, "%s.bin", sim->profiler.prof_func[i].name);
      end_addr = extract_function (sim, tmp1, start_addr);

      log ("Testing function %s (%08lx - %08lx)\n", sim->profiler.prof_func[i].name,
	   start_addr, end_addr);
      PRINTF ("Testing function %s (%08lx - %08lx)\n", sim->profiler.prof_func[i].name,
	      start_addr, end_addr);
      sim->func[i] =
	analyse_function (sim, sim->profiler.prof_func[i].name, orig_time, start_addr, end_addr,
			  sim->config.cuc.memory_order, sim->profiler.prof_func[i].calls);
      sim->func_v[i] = 0;
    }
  set_func_deps (sim);

  while (1)
    {
      char *s;
    wait_command:
      PRINTF ("(cuc) ");
      fflush (stdout);
    wait_command_empty:
      s = fgets (tmp1, sizeof tmp1, stdin);
      usleep (100);
      if (!s)
	goto wait_command_empty;
      for (s = tmp1; *s != '\0' && *s != '\n' && *s != '\r'; s++);
      *s = '\0';

      /* quit command */
      if (strcmp (tmp1, "q") == 0 || strcmp (tmp1, "quit") == 0)
	{
	  /* Delete temporary files */
	  for (i = 0; i < sim->profiler.prof_nfuncs - 1; i++)
	    {
	      sprintf (tmp1, "%s.bin", sim->profiler.prof_func[i].name);
	      log ("Deleting temporary file %s %s\n", tmp1,
		   remove (tmp1) ? "FAILED" : "OK");
	      sprintf (tmp1, "%s.bin.bb", sim->profiler.prof_func[i].name);
	      log ("Deleting temporary file %s %s\n", tmp1,
		   remove (tmp1) ? "FAILED" : "OK");
	    }
	  break;

	  /* profile command */
	}
      else if (strcmp (tmp1, "p") == 0 || strcmp (tmp1, "profile") == 0)
	{
	  int ntime = 0;
	  int size = 0;
	  PRINTF
	    ("-----------------------------------------------------------------------------\n");
	  PRINTF
	    ("|function name       |calls|avg cycles  |old%%| max. f.  | impr. f.| options |\n");
	  PRINTF
	    ("|--------------------+-----+------------+----+----------|---------+---------|\n");
	  for (j = 0; j < sim->profiler.prof_nfuncs; j++)
	    {
	      int bestcyc = 0, besti = 0;
	      char tmp[100];
	      for (i = 0; i < sim->profiler.prof_nfuncs; i++)
		if (sim->profiler.prof_func[i].cum_cycles > bestcyc)
		  {
		    bestcyc = sim->profiler.prof_func[i].cum_cycles;
		    besti = i;
		  }
	      i = besti;
	      PRINTF ("|%-20s|%5li|%12.1f|%3.0f%%| ",
		      strstrip (tmp, sim->profiler.prof_func[i].name, 20),
		      sim->profiler.prof_func[i].calls,
		      ((double) sim->profiler.prof_func[i].cum_cycles / sim->profiler.prof_func[i].calls),
		      (100. * sim->profiler.prof_func[i].cum_cycles / sim->profiler.prof_cycles));
	      if (sim->func[i])
		{
		  double f = 1.0;
		  if (sim->func_v[i])
		    {
		      int nt = calc_cycles (sim->func[i]);
		      int s = calc_size (sim->func[i]);
		      f = 1. * sim->func[i]->orig_time / nt;
		      ntime += nt;
		      size += s;
		    }
		  else
		    ntime += sim->profiler.prof_func[i].cum_cycles;
		  PRINTF ("%8.1f |%8.1f | %-8s|\n",
			  1.f * sim->profiler.prof_func[i].cum_cycles /
			  sim->func[i]->timings.new_time, f,
			  format_func_options (tmp, sim->func[i]));
		}
	      else
		{
		  PRINTF ("     N/A |     N/A |     N/A |\n");
		  ntime += sim->profiler.prof_func[i].cum_cycles;
		}
	      sim->profiler.prof_func[i].cum_cycles = -sim->profiler.prof_func[i].cum_cycles;
	    }
	  for (i = 0; i < sim->profiler.prof_nfuncs; i++)
	    sim->profiler.prof_func[i].cum_cycles = -sim->profiler.prof_func[i].cum_cycles;
	  PRINTF
	    ("-----------------------------------------------------------------------------\n");
	  PRINTF
	    ("Total %i cycles (was %i), total added gates = %i. Speed factor %.1f\n",
	     ntime, sim->profiler.prof_cycles, size, 1. * sim->profiler.prof_cycles / ntime);

	  /* debug command */
	}
      else if (strncmp (tmp1, "d", 1) == 0 || strncmp (tmp1, "debug", 5) == 0)
	{
	  sscanf (tmp1, "%*s %i", &sim->cuc_debug);
	  if (sim->cuc_debug < 0)
	    sim->cuc_debug = 0;
	  if (sim->cuc_debug > 9)
	    sim->cuc_debug = 9;

	  /* generate command */
	}
      else if (strcmp (tmp1, "g") == 0 || strcmp (tmp1, "generate") == 0)
	{
	  /* check for function dependencies */
	  for (i = 0; i < sim->profiler.prof_nfuncs; i++)
	    if (sim->func[i])
	      sim->func[i]->tmp = sim->func_v[i];
	  for (i = 0; i < sim->profiler.prof_nfuncs; i++)
	    if (sim->func[i])
	      for (j = 0; j < sim->func[i]->nfdeps; j++)
		if (!sim->func[i]->fdeps[j] || !sim->func[i]->fdeps[j]->tmp)
		  {
		    PRINTF
		      ("Function %s must be selected for translation (required by %s)\n",
		       sim->profiler.prof_func[j].name, sim->profiler.prof_func[i].name);
		    goto wait_command;
		  }
	  for (i = 0; i < sim->profiler.prof_nfuncs; i++)
	    if (sim->func[i] && sim->func_v[i])
	      generate_function (sim, sim->func[i], sim->profiler.prof_func[i].name, filename_cut);
	  generate_main (sim->profiler.prof_nfuncs, sim->func, filename_cut);

	  /* list command */
	}
      else if (strcmp (tmp1, "l") == 0 || strcmp (tmp1, "list") == 0)
	{
	  /* check for function dependencies */
	  for (i = 0; i < sim->profiler.prof_nfuncs; i++)
	    if (sim->func_v[i])
	      {
		PRINTF ("%s\n", sim->profiler.prof_func[i].name);
	      }

	  /* selectall command */
	}
      else if (strcmp (tmp1, "sa") == 0 || strcmp (tmp1, "selectall") == 0)
	{
	  int f;
	  for (f = 0; f < sim->profiler.prof_nfuncs; f++)
	    if (sim->func[f])
	      {
		sim->func_v[f] = 1;
		PRINTF ("Function %s selected for translation.\n",
			sim->profiler.prof_func[f].name);
	      }

	  /* select command */
	}
      else if (strncmp (tmp1, "s", 1) == 0
	       || strncmp (tmp1, "select", 6) == 0)
	{
	  char tmp[50], ch;
	  int p, o, b, f;
	  p = sscanf (tmp1, "%*s %s %i%c", tmp, &b, &ch);
	  if (p < 1)
	    PRINTF ("Invalid parameters.\n");
	  else
	    {
	      /* Check if we have valid option */
	      for (f = 0; f < sim->profiler.prof_nfuncs; f++)
		if (strcmp (sim->profiler.prof_func[f].name, tmp) == 0 && sim->func[f])
		  break;
	      if (f < sim->profiler.prof_nfuncs)
		{
		  if (p == 1)
		    {
		      if (sim->func[f])
			{
			  sim->func_v[f] = 1;
			  PRINTF ("Function %s selected for translation.\n",
				  sim->profiler.prof_func[f].name);
			}
		      else
			PRINTF ("Function %s not suitable for translation.\n",
				sim->profiler.prof_func[f].name);
		    }
		  else
		    {
		      if (!sim->func_v[f])
			PRINTF
			  ("Function %s not yet selected for translation.\n",
			   sim->profiler.prof_func[f].name);
		      if (p < 3)
			goto invalid_option;
		      for (o = 0;
			   option_char[o] != '\0' && option_char[o] != ch;
			   o++);
		      if (!option_char[o])
			goto invalid_option;
		      if (b < 0 || b >= sim->func[f]->num_bb)
			goto invalid_option;
		      if (o < 0 || o >= sim->func[f]->bb[b].ntim)
			goto invalid_option;

		      /* select an option */
		      sim->func[f]->bb[b].selected_tim = o;
		      if (sim->func[f]->bb[b].tim[o].nshared)
			{
			  PRINTF ("Option has shared instructions: ");
			  print_shared (sim, sim->func[f], sim->func[f]->bb[b].tim[o].shared,
					sim->func[f]->bb[b].tim[o].nshared);
			  PRINTF ("\n");
			}
		      goto wait_command;
		    invalid_option:
		      PRINTF ("Invalid option.\n");
		    }
		}
	      else
		PRINTF ("Invalid function.\n");
	    }

	  /* unselect command */
	}
      else if (strncmp (tmp1, "u", 1) == 0
	       || strncmp (tmp1, "unselect", 8) == 0)
	{
	  char tmp[50], ch;
	  int p, o, b, f;
	  p = sscanf (tmp1, "%*s %s %i%c", tmp, &b, &ch);
	  if (p < 1)
	    PRINTF ("Invalid parameters.\n");
	  else
	    {
	      /* Check if we have valid option */
	      for (f = 0; f < sim->profiler.prof_nfuncs; f++)
		if (strcmp (sim->profiler.prof_func[f].name, tmp) == 0 && sim->func[f])
		  break;
	      if (f < sim->profiler.prof_nfuncs)
		{
		  if (p == 1)
		    {
		      if (sim->func[f])
			{
			  sim->func_v[f] = 0;
			  PRINTF ("Function %s unselected for translation.\n",
				  sim->profiler.prof_func[f].name);
			}
		      else
			PRINTF ("Function %s not suitable for translation.\n",
				sim->profiler.prof_func[f].name);
		    }
		  else
		    {
		      if (p < 3)
			goto invalid_option;
		      for (o = 0;
			   option_char[o] != '\0' && option_char[o] != ch;
			   o++);
		      if (!option_char[o])
			goto invalid_option;
		      if (b < 0 || b >= sim->func[f]->num_bb)
			goto invalid_option;
		      if (o < 0 || o >= sim->func[f]->bb[b].ntim)
			goto invalid_option;

		      /* select an option */
		      sim->func[f]->bb[b].selected_tim = -1;
		    }
		}
	      else
		PRINTF ("Invalid function.\n");
	    }

	  /* options command */
	}
      else if (strcmp (tmp1, "o") == 0 || strcmp (tmp1, "options") == 0)
	{
	  int any = 0;
	  PRINTF ("Available options:\n");
	  for (i = 0; i < sim->profiler.prof_nfuncs; i++)
	    if (sim->func[i])
	      {
		options_cmd (sim, i, sim->func[i]);
		any = 1;
	      }
	  if (any)
	    PRINTF
	      ("-----------------------------------------------------------------------------\n");
	  else
	    PRINTF ("Sorry. No available options.\n");

	  /* Ignore empty string */
	}
      else if (strcmp (tmp1, "") == 0)
	{

	  /* help command */
	}
      else
	{
	  if (strcmp (tmp1, "h") != 0 && strcmp (tmp1, "help") != 0)
	    PRINTF ("Unknown command.\n");
	  PRINTF ("OpenRISC Custom Unit Compiler command prompt\n");
	  PRINTF ("Available commands:\n");
	  PRINTF ("  h | help                   displays this help\n");
	  PRINTF ("  q | quit                   returns to or1ksim prompt\n");
	  PRINTF
	    ("  p | profile                displays function profiling\n");
	  PRINTF ("  d | debug #                sets debug level (0-9)\n");
	  PRINTF
	    ("  o | options                displays available options\n");
	  PRINTF
	    ("  s | select func [option]   selects an option/function\n");
	  PRINTF
	    ("  u | unselect func [option] unselects an option/function\n");
	  PRINTF ("  g | generate               generates verilog file\n");
	  PRINTF
	    ("  l | list                   displays selected functions\n");
	}
    }

  /* Dispose memory */
  for (i = 0; i < sim->profiler.prof_nfuncs - 1; i++)
    if (sim->func[i])
      free_func (sim->func[i]);

  fclose (sim->flog);
}

/*----------------------------------------------------[ CUC Configuration ]---*/

/*---------------------------------------------------------------------------*/
/*!Set the memory order

   Value must be one of none, weak, strong or exact. Invalid values are
   ignored with a warning.

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
cuc_memory_order (or1ksim *sim, union param_val val, void *dat)
{
  if (strcasecmp (val.str_val, "none") == 0)
    {
      sim->config.cuc.memory_order = MO_NONE;
    }
  else if (strcasecmp (val.str_val, "weak") == 0)
    {
      sim->config.cuc.memory_order = MO_WEAK;
    }
  else if (strcasecmp (val.str_val, "strong") == 0)
    {
      sim->config.cuc.memory_order = MO_STRONG;
    }
  else if (strcasecmp (val.str_val, "exact") == 0)
    {
      sim->config.cuc.memory_order = MO_EXACT;
    }
  else
    {
      fprintf (stderr, "Warning: CUC memory order invalid. Ignored");
    }
}				/* cuc_memory_order() */


static void
cuc_calling_conv (or1ksim *sim, union param_val val, void *dat)
{
  sim->config.cuc.calling_convention = val.int_val;
}

static void
cuc_enable_bursts (or1ksim *sim, union param_val val, void *dat)
{
  sim->config.cuc.enable_bursts = val.int_val;
}

static void
cuc_no_multicycle (or1ksim *sim, union param_val val, void *dat)
{
  sim->config.cuc.no_multicycle = val.int_val;
}


/*---------------------------------------------------------------------------*/
/*!Set the timings file

   Free any existing string.

   @param[in] val  The value to use
   @param[in] dat  The config data structure (not used here)                 */
/*---------------------------------------------------------------------------*/
static void
cuc_timings_fn (or1ksim *sim, union param_val val, void *dat)
{
  if (NULL != sim->config.cuc.timings_fn)
    {
      free (sim->config.cuc.timings_fn);
    }

  sim->config.cuc.timings_fn = strdup (val.str_val);

}				/* cuc_timings_fn() */


void
reg_cuc_sec (or1ksim *sim)
{
  struct config_section *sec = reg_config_sec (sim, "cuc", NULL, NULL);

  reg_config_param (sec, "memory_order", paramt_word, cuc_memory_order);
  reg_config_param (sec, "calling_convention", paramt_int, cuc_calling_conv);
  reg_config_param (sec, "enable_bursts", paramt_int, cuc_enable_bursts);
  reg_config_param (sec, "no_multicycle", paramt_int, cuc_no_multicycle);
  reg_config_param (sec, "timings_file", paramt_str, cuc_timings_fn);
  reg_config_param (sec, "timings_fn", paramt_str, cuc_timings_fn);
}
