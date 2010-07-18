/* profiler.c -- profiling utility

   Copyright (C) 2001 Marko Mlinar, markom@opencores.org
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

/* Command line utility, that displays profiling information, generated
   by or1ksim. (use profile command interactively, when running or1ksim, or
   separate psim command).  */

#include <stdlib.h>

/* Autoconf and/or portability configuration */
#include "config.h"
#include "port.h"

/* Package includes */
#include "profiler.h"
#include "sim-config.h"
#include "argtable2.h"

/*---------------------------------------------------------------------------*/
/*! Acquire data from profiler file

  @param[in] fprofname   Data file to analyse

  @return  0 on success, return code otherwise                               */
/*---------------------------------------------------------------------------*/
int
prof_acquire (or1ksim *sim, const char *fprofname)
{
  int line = 0;
  int reopened = 0;

  if (sim->runtime.sim.fprof)
    {
      sim->profiler.fprof = sim->runtime.sim.fprof;
      reopened = 1;
      rewind (sim->profiler.fprof);
    }
  else
    sim->profiler.fprof = fopen (fprofname, "rt");

  if (!sim->profiler.fprof)
    {
      fprintf (stderr, "Cannot open profile file: %s\n", fprofname);
      return 1;
    }

  while (1)
    {
      char dir = fgetc (sim->profiler.fprof);
      line++;
      if (dir == '+')
	{
	  if (fscanf
	      (sim->profiler.fprof, "%08X %08X %08X %s\n", &sim->profiler.stack[sim->profiler.nstack].cycles,
	       &sim->profiler.stack[sim->profiler.nstack].raddr, &sim->profiler.stack[sim->profiler.nstack].addr,
	       &sim->profiler.stack[sim->profiler.nstack].name[0]) != 4)
	    fprintf (stderr, "Error reading line #%i\n", line);
	  else
	    {
	      sim->profiler.prof_cycles = sim->profiler.stack[sim->profiler.nstack].cycles;
	      sim->profiler.nstack++;
	      if (sim->profiler.nstack > sim->profiler.maxstack)
		sim->profiler.maxstack = sim->profiler.nstack;
	    }
	  sim->profiler.ntotcalls++;
	}
      else if (dir == '-')
	{
	  struct stack_struct s;
	  if (fscanf (sim->profiler.fprof, "%08X %08X\n", &s.cycles, &s.raddr) != 2)
	    fprintf (stderr, "Error reading line #%i\n", line);
	  else
	    {
	      int i;
	      sim->profiler.prof_cycles = s.cycles;
	      for (i = sim->profiler.nstack - 1; i >= 0; i--)
		if (sim->profiler.stack[i].raddr == s.raddr)
		  break;
	      if (i >= 0)
		{
		  /* pop everything above current from sim->profiler.,
		     if more than one, something went wrong */
		  while (sim->profiler.nstack > i)
		    {
		      int j;
		      long time;
		      sim->profiler.nstack--;
		      time = s.cycles - sim->profiler.stack[sim->profiler.nstack].cycles;
		      if (!sim->profiler.quiet && time < 0)
			{
			  fprintf (stderr,
				   "WARNING: Negative time at %s (return addr = %08X).\n",
				   sim->profiler.stack[i].name, sim->profiler.stack[i].raddr);
			  time = 0;
			}

		      /* Whether in normal mode, we must substract called function from execution time.  */
		      if (!sim->profiler.cumulative)
			for (j = 0; j < sim->profiler.nstack; j++)
			  sim->profiler.stack[j].cycles += time;

		      if (!sim->profiler.quiet && i != sim->profiler.nstack)
			fprintf (stderr,
				 "WARNING: Missaligned return call for %s (%08X) (found %s @ %08X), closing.\n",
				 sim->profiler.stack[sim->profiler.nstack].name, sim->profiler.stack[sim->profiler.nstack].raddr,
				 sim->profiler.stack[i].name, sim->profiler.stack[i].raddr);

		      for (j = 0; j < sim->profiler.prof_nfuncs; j++)
			if (sim->profiler.stack[sim->profiler.nstack].addr == sim->profiler.prof_func[j].addr)
			  {	/* function exists, append. */
			    sim->profiler.prof_func[j].cum_cycles += time;
			    sim->profiler.prof_func[j].calls++;
			    sim->profiler.nfunccalls++;
			    break;
			  }
		      if (j >= sim->profiler.prof_nfuncs)
			{	/* function does not yet exist, create new. */
			  sim->profiler.prof_func[sim->profiler.prof_nfuncs].cum_cycles = time;
			  sim->profiler.prof_func[sim->profiler.prof_nfuncs].calls = 1;
			  sim->profiler.nfunccalls++;
			  sim->profiler.prof_func[sim->profiler.prof_nfuncs].addr = sim->profiler.stack[sim->profiler.nstack].addr;
			  strcpy (sim->profiler.prof_func[sim->profiler.prof_nfuncs].name,
				  sim->profiler.stack[sim->profiler.nstack].name);
			  sim->profiler.prof_nfuncs++;
			}
		    }
		}
	      else if (!sim->profiler.quiet)
		fprintf (stderr,
			 "WARNING: Cannot find return call for (%08X), ignoring.\n",
			 s.raddr);
	    }
	}
      else
	break;
    }

  /* If we have reopened the file, we need to add end of "[outside functions]" */
  if (reopened)
    {
      sim->profiler.prof_cycles = sim->runtime.sim.cycles;
      /* pop everything above current from sim->profiler.,
         if more than one, something went wrong */
      while (sim->profiler.nstack > 0)
	{
	  int j;
	  long time;
	  sim->profiler.nstack--;
	  time = sim->runtime.sim.cycles - sim->profiler.stack[sim->profiler.nstack].cycles;
	  /* Whether in normal mode, we must substract called function from execution time.  */
	  if (!sim->profiler.cumulative)
	    for (j = 0; j < sim->profiler.nstack; j++)
	      sim->profiler.stack[j].cycles += time;

	  for (j = 0; j < sim->profiler.prof_nfuncs; j++)
	    if (sim->profiler.stack[sim->profiler.nstack].addr == sim->profiler.prof_func[j].addr)
	      {			/* function exists, append. */
		sim->profiler.prof_func[j].cum_cycles += time;
		sim->profiler.prof_func[j].calls++;
		sim->profiler.nfunccalls++;
		break;
	      }
	  if (j >= sim->profiler.prof_nfuncs)
	    {			/* function does not yet exist, create new. */
	      sim->profiler.prof_func[sim->profiler.prof_nfuncs].cum_cycles = time;
	      sim->profiler.prof_func[sim->profiler.prof_nfuncs].calls = 1;
	      sim->profiler.nfunccalls++;
	      sim->profiler.prof_func[sim->profiler.prof_nfuncs].addr = sim->profiler.stack[sim->profiler.nstack].addr;
	      strcpy (sim->profiler.prof_func[sim->profiler.prof_nfuncs].name, sim->profiler.stack[sim->profiler.nstack].name);
	      sim->profiler.prof_nfuncs++;
	    }
	}
    }
  else
    fclose (sim->profiler.fprof);
  return 0;
}

/* Print out profiling data */
static void
prof_print (or1ksim *sim)
{
  int i, j;
  if (sim->profiler.cumulative)
    PRINTF ("CUMULATIVE TIMES\n");
  PRINTF
    ("---------------------------------------------------------------------------\n");
  PRINTF
    ("|function name            |addr    |# calls |avg cycles  |total cyles     |\n");
  PRINTF
    ("|-------------------------+--------+--------+------------+----------------|\n");
  for (j = 0; j < sim->profiler.prof_nfuncs; j++)
    {
      int bestcyc = 0, besti = 0;
      for (i = 0; i < sim->profiler.prof_nfuncs; i++)
	if (sim->profiler.prof_func[i].cum_cycles > bestcyc)
	  {
	    bestcyc = sim->profiler.prof_func[i].cum_cycles;
	    besti = i;
	  }
      i = besti;
      PRINTF ("| %-24s|%08X|%8li|%12.1f|%11li,%3.0f%%|\n",
	      sim->profiler.prof_func[i].name, sim->profiler.prof_func[i].addr, sim->profiler.prof_func[i].calls,
	      ((double) sim->profiler.prof_func[i].cum_cycles / sim->profiler.prof_func[i].calls),
	      sim->profiler.prof_func[i].cum_cycles,
	      (100. * sim->profiler.prof_func[i].cum_cycles / sim->profiler.prof_cycles));
      sim->profiler.prof_func[i].cum_cycles = -1;
    }
  PRINTF
    ("---------------------------------------------------------------------------\n");
  PRINTF ("Total %i functions, %i cycles.\n", sim->profiler.prof_nfuncs, sim->profiler.prof_cycles);
  PRINTF ("Total function calls %i/%i (max depth %i).\n", sim->profiler.nfunccalls,
	  sim->profiler.ntotcalls, sim->profiler.maxstack);
}

/* Set options */
void
prof_set (or1ksim *sim, int _quiet, int _cumulative)
{
  sim->profiler.quiet = _quiet;
  sim->profiler.cumulative = _cumulative;
}

/*---------------------------------------------------------------------------*/
/*! Parse the arguments for the profiling utility

    Updated by Jeremy Bennett to use argtable2. Also has an option just to
    print help, for use with the CLI.

    @param[in] argc       Number of command args
    @param[in] argv       Vector of the command args
    @param[in] just_help  If 1 (true), ignore argc & argv and just print out
                          the help message without parsing args

    @return  0 on success, 1 on failure                                      */
/*---------------------------------------------------------------------------*/
int
main_profiler (or1ksim* sim, int argc, char *argv[], int just_help)
{
  struct arg_lit *vercop;
  struct arg_lit *help;
  struct arg_lit *cum_arg;
  struct arg_lit *quiet_arg;
  struct arg_file *gen_file;
  struct arg_end *end;

  void *argtab[6];
  int nerrors;

  /* Specify each argument, with fallback values */
  vercop = arg_lit0 ("v", "version", "version and copyright notice");
  help = arg_lit0 ("h", "help", "print this help message");
  cum_arg = arg_lit0 ("c", "sim->profiler.cumulative",
		      "sim->profiler.cumulative sum of cycles in functions");
  quiet_arg = arg_lit0 ("q", "sim->profiler.quiet", "suppress messages");
  gen_file = arg_file0 ("g", "generate", "<file>",
			"data file to analyse (default " "sim.profile)");
  gen_file->filename[0] = "sim.profile";
  end = arg_end (20);

  /* Set up the argument table */
  argtab[0] = vercop;
  argtab[1] = help;
  argtab[2] = cum_arg;
  argtab[3] = quiet_arg;
  argtab[4] = gen_file;
  argtab[5] = end;

  /* If we are just asked for a help message, then we don't parse the
     args. This is used to implement the help function from the CLI. */
  if (just_help)
    {
      printf ("profile");
      arg_print_syntax (stdout, argtab, "\n");
      arg_print_glossary (stdout, argtab, "  %-25s %s\n");

      arg_freetable (argtab, sizeof (argtab) / sizeof (argtab[0]));
      return 0;
    }

  /* Parse */
  nerrors = arg_parse (argc, argv, argtab);

  /* Special case here is if help or version is specified, we ignore any other
     errors and just print the help or version information and then give up. */
  if (vercop->count > 0)
    {
      PRINTF ("OpenRISC 1000 Profiling Utility, version %s\n",
	      PACKAGE_VERSION);

      arg_freetable (argtab, sizeof (argtab) / sizeof (argtab[0]));
      return 0;
    }

  if (help->count > 0)
    {
      printf ("Usage: %s ", argv[0]);
      arg_print_syntax (stdout, argtab, "\n");
      arg_print_glossary (stdout, argtab, "  %-25s %s\n");

      arg_freetable (argtab, sizeof (argtab) / sizeof (argtab[0]));
      return 0;
    }

  /* Deal with any errors */
  if (0 != nerrors)
    {
      arg_print_errors (stderr, end, "profile");
      fprintf (stderr, "Usage: %s ", argv[0]);
      arg_print_syntaxv (stderr, argtab, "\n");

      arg_freetable (argtab, sizeof (argtab) / sizeof (argtab[0]));
      return 1;
    }

  /* Cumulative result wanted? */
  sim->profiler.cumulative = cum_arg->count;

  /* Suppress messages? */
  sim->profiler.quiet = quiet_arg->count;

  /* Get the profile from the file */
  prof_acquire (sim, gen_file->filename[0]);

  /* Now we have all data acquired. Print out. */
  prof_print (sim);

  arg_freetable (argtab, sizeof (argtab) / sizeof (argtab[0]));
  return 0;

}				/* main_profiler() */
