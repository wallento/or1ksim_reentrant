/* toplevel.c -- Top level simulator support source file

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

/* System includes */
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
// #include <pthread.h> // TODO: problem with include

/* Package includes */
#include "toplevel-support.h"
#include "sim-config.h"
#include "debug-unit.h"
#include "sim-cmd.h"
#include "sched.h"
#include "tick.h"
#include "pm.h"
#include "pic.h"
#include "execute.h"
#include "labels.h"
#include "stats.h"
#include "opcode/or32.h"
#include "parse.h"
#include "gdbcomm.h"
#include "rsp-server.h"
#include "vapi.h"
#include "abstract.h"
#include "mc.h"
#include "except.h"

#include "fd.h"
#include "file.h"
#include "tcp.h"
#include "xterm.h"
#include "tty.h"

/*! Struct for list of reset hooks */
struct sim_reset_hook
{
  void *dat;
  void (*reset_hook) (or1ksim *sim, void *);
  struct sim_reset_hook *next;
};

/*!
 * Book-keeping of running instances, that is guarded by a mutex to increase thread-safety
 */
struct simulations_nodes
{
	or1ksim *sim;
	struct simulations_nodes *next;
};

static struct simulations_nodes *simulations = NULL; /*!< Root of global list of registered instances */
pthread_mutex_t simulations_mutex;                   /*!< Mutex to secure the access to the list of instances */ 

/*!Register simulation in global list
 * 
 * Add the simulator instance to the global list.
 * @param sim Simulator instance
 * @return Success of operation, 0 on success
 */
int
sim_register( or1ksim *sim )
{
	if ( !simulations )
	  {
	  	pthread_mutex_init( &simulations_mutex, NULL );
	  }

	struct simulations_nodes *new = malloc( sizeof( struct simulations_nodes ) );
	if ( !new )
	  {
		fprintf( stderr, "Insufficient memory.\n" );
		exit( 1 );
	  }

	pthread_mutex_lock( &simulations_mutex );
	new->sim = sim;
	new->next = simulations;
	simulations = new;
	pthread_mutex_unlock( &simulations_mutex );

	return 0;
}

/*!Unregister simulator instance
 * 
 * Remove simulator instance from global list.
 * @param sim Simulator instance to remove
 * @return Success of operation, 0 on success
 */
int
sim_unregister( or1ksim *sim ) 
{
	pthread_mutex_lock( &simulations_mutex );

	struct simulations_nodes *prev = NULL;
	struct simulations_nodes *it   = simulations;

	for (; it != NULL; it = it->next )
	  {
	  	if ( it->sim == sim )
	  	  {
	  	  	if ( prev )
	  	  	  {
	  	  		prev->next = it->next;
	  	  	  }
	  	  	else
	  	  	  {
	  	  	  	simulations = it->next;
	  	  	  }
	  	  	
	  	  	free( it );
	  	  }
	  }

	pthread_mutex_unlock( &simulations_mutex );
	
	return 0;
}

/*---------------------------------------------------------------------------*/
/*!Signal handler for ctrl-C

   Sets the iprompt flag, so the simulator will stop next time round the
   loop. If the iprompt flag is set when we enter here, that means the
   simulator has not reacted since the last ctrl-C, so we kill the simulation.

   @param[in] signum  The signal which triggered this handler                */
/*---------------------------------------------------------------------------*/
void
ctrl_c (signum)
     int signum;
{
  /* In case the user pressed ctrl+c twice without the sim reacting kill it.
   * This is incase the sim locks up in a high level routine, without executeing
   * any (or) code */

	if ( !simulations )
	  {
	 	exit( -1 );
	  }

	if ( simulations->next )
	  {
		fprintf( stderr, "Only one or1ksim instance can be handled interactively\n" );
	  }

	pthread_mutex_lock( &simulations_mutex );

	if (simulations->sim->runtime.sim.iprompt && !simulations->sim->runtime.sim.iprompt_run)
	  {
			sim_done (simulations->sim);
			exit(-1);
	  }
  /* Mark the simulator to stop next time and reinstall the handler */
  simulations->sim->runtime.sim.iprompt = 1;
  signal (SIGINT, ctrl_c);

	  pthread_mutex_unlock( &simulations_mutex );
}	/* ctrl_c() */


/*---------------------------------------------------------------------------*/
/*!Routine poll to see if interaction is needed

   This is most likely to happen due to a ctrl-C. However when the -i flag is
   specified, the simulator starts up ready for interaction.

   The main simulator loop will stop for interaction if it hits a breakpoint.

   @param[in] dat  Data passed in by the Or1ksim scheduler. Not needed by this
   function.                                                                 */
/*---------------------------------------------------------------------------*/
void
check_int (or1ksim *sim, void *dat)
{
  if (sim->runtime.sim.iprompt)
    {
      set_stall_state (sim, 0);
      handle_sim_command (sim);
    }

  SCHED_ADD (check_int, NULL, CHECK_INT_TIME);

}	/* check_int() */

/*!Load simulation defaults
 * 
 * Load the simulation defaults, that were used for the global variables, to the struct elements.
 * @param sim Simulation instance
 */
void
sim_defaults( or1ksim *sim )
{
	sim->sections         = NULL;
	sim->sbuf_wait_cyc    = 0;
	sim->sbuf_total_cyc   = 0;
	sim->do_stats         = 0;
	sim->hist_exec_tail   = NULL;
	sim->issued_per_cycle = 4;
	sim->sbuf_head        = 0;
	sim->sbuf_tail        = 0;
	sim->sbuf_count       = 0;
	#if !(DYNAMIC_EXECUTION)
	memset( &(sim->sbuf_buf), 0, MAX_SBUF_LEN * sizeof(int) );
	#endif
	sim->sbuf_prev_cycles = 0;
	sim->sim_reset_hooks  = NULL;
	sim->mc_area          = NULL;
#if IMM_STATS
	memset( &sim->bcnt, 0, 33*3*sizeof(int) );
	memset( &sim->bsum, 0, 3*sizeof(int) ) ;
	sim->movhi = 0;
#endif
	memset( &sim->range_cache, 0, 256*sizeof(int) );
	sim->ic_state           = NULL;
	sim->curpass            = 0;
	sim->disassembled       = &sim->disassembled_str[0];
	sim->cuc_debug          = 0;
	sim->current_scan_chain = JTAG_CHAIN_GLOBAL;
	sim->in_reset           = 0;
	sim->server_ip          = 0;
	sim->server_port        = 0;
	sim->server_fd          = 0;
	sim->gdb_fd             = 0;
	sim->tcp_level          = 0;
	sim->preloaded[0].name  = "fd";
	sim->preloaded[0].ops   = &fd_channel_ops;
	sim->preloaded[0].next  = &sim->preloaded[1];
	sim->preloaded[1].name  = "file";
	sim->preloaded[1].ops   = &file_channel_ops;
	sim->preloaded[1].next  = &sim->preloaded[2];
	sim->preloaded[2].name  = "xterm";
	sim->preloaded[2].ops   = &xterm_channel_ops;
	sim->preloaded[2].next  = &sim->preloaded[3];
	sim->preloaded[3].name  = "tcp";
	sim->preloaded[3].ops   = &tcp_channel_ops;
	sim->preloaded[3].next  = &sim->preloaded[4];
	sim->preloaded[4].name  = "tty";
	sim->preloaded[4].ops   = &tty_channel_ops;
	sim->preloaded[4].next  = NULL;
	sim->head               = &sim->preloaded[0];
	sim->dmas               = NULL;
	sim->mc_areas           = NULL;
	sim->mcs                = NULL;
//	sim->pic_state_int.edge_trigger = 1;
//	sim->pic_state_int.enabled = 1;
//	sim->pic_state = &sim->pic_state_int;
#define DECLARE_DEBUG_CHANNEL(dbch) sim->__orsim_dbch_##dbch = "\0"#dbch;
#include "dbchs.h"
#undef DECLARE_DEBUG_CHANNEL

	unsigned int channel_cnt = 0;
#define DECLARE_DEBUG_CHANNEL(dbch) channel_cnt++;
#include "dbchs.h"
#undef DECLARE_DEBUG_CHANNEL

	sim->__orsim_dbchs = malloc( sizeof( char* ) * channel_cnt+1 );

	channel_cnt = 0; /* local variable for counting and indexing */

#define DECLARE_DEBUG_CHANNEL(dbch)	sim->__orsim_dbchs[channel_cnt] = sim->__orsim_dbch_##dbch; channel_cnt++;
#include "dbchs.h"
#undef DECLARE_DEBUG_CHANNEL
	sim->__orsim_dbchs[channel_cnt] = NULL;

	sim->cycles_start   = 0;

	sim->except_pending = 0;
	sim->audio_cnt      = 0;
	sim->fo             = 0;

	sim->profiler.prof_nfuncs = 0;
	sim->profiler.prof_cycles = 0;
	sim->profiler.nstack      = 0;
	sim->profiler.maxstack    = 0;
	sim->profiler.ntotcalls   = 0;
	sim->profiler.nfunccalls  = 0;
	sim->profiler.cumulative  = 0;
	sim->profiler.quiet       = 0;
	sim->profiler.fprof       = 0;

	sim->mprofiler.group_bits = 2;
	sim->mprofiler.start_addr = 0;
	sim->mprofiler.end_addr   = 0xffffffff;
	sim->mprofiler.fprof      = 0;

	sim->sim_stats = NULL;
}

/*!Create simulator instance
 * 
 * Creates a simulator instance and loads it with the default values.
 * @return Pointer to the new instance
 */
or1ksim*
sim_instance()
{
	or1ksim* sim = malloc( sizeof( or1ksim ) );
	sim_defaults( sim );
	return sim;
}

/*!Free the simulator and the space allocated for it
 * 
 * Free all allocated memory for the simulator variables and finally the simulator itself
 * @param sim Simulator instance
 */
void
sim_free( or1ksim *sim )
{
	free( sim );
}

/*---------------------------------------------------------------------------*/
/*!Register a new reset hook

   The registered functions will be called in turn, whenever the simulation is
   reset by calling sim_reset().

   @param[in] reset_hook  The function to be called on reset
   @param[in] dat         The data structure to be passed as argument when the
                          reset_hook function is called.                     */
/*---------------------------------------------------------------------------*/
void
reg_sim_reset (or1ksim *sim,void (*reset_hook) (or1ksim *sim,void *), void *dat)
{
  struct sim_reset_hook *new = malloc (sizeof (struct sim_reset_hook));

  if (!new)
    {
      fprintf (stderr, "reg_sim_reset: Out-of-memory\n");
      exit (1);
    }

  new->dat = dat;
  new->reset_hook = reset_hook;
  new->next = sim->sim_reset_hooks;
  sim->sim_reset_hooks = new;

}	/* reg_sim_reset() */


/*---------------------------------------------------------------------------*/
/*!Reset the simulator

   The scheduler is reset, then all reset functions on the reset hook list
   (i.e. peripherals) are reset. Then standard core functions (which do not
   use reset hooks) are reset: tick timer, power management, programmable
   interrupt controller and debug unit.

   The scheduler queue is reinitialized with an immediate check for ctrl-C on
   its queue.

   Finally the count of simulated cycles is set to zero. and the CPU itself is
   reset.                                                                    */
/*---------------------------------------------------------------------------*/
void
sim_reset (or1ksim *sim)
{
  struct sim_reset_hook *cur_reset = sim->sim_reset_hooks;

  /* We absolutely MUST reset the scheduler first */
  sched_reset (sim);

  while (cur_reset)
    {
      cur_reset->reset_hook (sim, cur_reset->dat);
      cur_reset = cur_reset->next;
    }

  tick_reset (sim);
  pm_reset (sim);
  pic_reset (sim);
  du_reset (sim);

  /* Make sure that runtime.sim.iprompt is the first thing to get checked */
  SCHED_ADD (check_int, NULL, 1);

  /* FIXME: Lame-ass way to get runtime.sim.mem_cycles not going into overly
   * negative numbers.  This happens because parse.c uses setsim_mem32 to load
   * the program but set_mem32 calls dc_simulate_write, which inturn calls
   * setsim_mem32.  This mess of memory statistics needs to be sorted out for
   * good one day */
  sim->runtime.sim.mem_cycles = 0;
  cpu_reset (sim);

}	/* sim_reset() */


/*---------------------------------------------------------------------------*/
/*!Initalize the simulator

  Reset internal data: symbol table (aka labels), breakpoints and
  stats. Rebuild the FSA's used for disassembly.

  Initialize the dynamic execution system if required.

  Initialize the scheduler.

  Open the various logs and statistics files requested by the configuration
  and/or command arguments.

  Initialize GDB and VAPI connections.

  Reset the simulator.

  Wait for VAPI to connect if configured.                                    */
/*---------------------------------------------------------------------------*/
void
sim_init (or1ksim *sim)
{
  printf ("Or1ksim " PACKAGE_VERSION "\n" );
  init_labels (sim);
  init_breakpoints (sim);
  initstats (sim);
  build_automata (sim);


#if DYNAMIC_EXECUTION
  /* Note: This must be called before the scheduler is used */
  init_dyn_recomp ();
#endif

  sched_init (sim);

  sim_reset (sim);			/* Must do this first - torches memory! */

  if (sim->config.sim.profile)
    {
      sim->runtime.sim.fprof = fopen (sim->config.sim.prof_fn, "wt+");
      if (!sim->runtime.sim.fprof)
	{
	  fprintf (stderr, "ERROR: sim_init: cannot open profile file %s: ",
		   sim->config.sim.prof_fn);
	  perror (NULL);
	  exit (1);
	}
      else
	fprintf (sim->runtime.sim.fprof,
		 "+00000000 FFFFFFFF FFFFFFFF [outside_functions]\n");
    }

  if (sim->config.sim.mprofile)
    {
      sim->runtime.sim.fmprof = fopen (sim->config.sim.mprof_fn, "wb+");
      if (!sim->runtime.sim.fmprof)
	{
	  fprintf (stderr, "ERROR: sim_init: cannot open memory profile "
		   "file %s: ", sim->config.sim.mprof_fn);
	  perror (NULL);
	  exit (1);
	}
    }

  if (sim->config.sim.exe_log)
    {
      sim->runtime.sim.fexe_log = fopen (sim->config.sim.exe_log_fn, "wt+");
      if (!sim->runtime.sim.fexe_log)
	{
	  fprintf (stderr, "sim_init: cannot open execution log file %s: ",
		   sim->config.sim.exe_log_fn);
	  perror (NULL);
	  exit (1);
	}
    }

  /* MM170901 always load at address zero */
  if (sim->runtime.sim.filename)
    {
      unsigned long endaddr = loadcode (sim, sim->runtime.sim.filename, 0, 0);

      if (endaddr == -1)
	{
	  fprintf (stderr, "ERROR: sim_init: problem loading code from %s\n",
		   sim->runtime.sim.filename);
	  exit (1);
	}
    }

  /* Disable GDB debugging, if debug unit is not available.  */
  if ((sim->config.debug.gdb_enabled || sim->config.debug.rsp_enabled) &&
      !sim->config.debug.enabled)
    {
      sim->config.debug.gdb_enabled = 0;
      sim->config.debug.rsp_enabled = 0;

      if (sim->config.sim.verbose)
	{
	  fprintf (stderr, "WARNING: sim_init: Debug module not enabled, "
		   "cannot start remote service to GDB\n");
	}
    }

  /* Start either RSP or legacy GDB debugging service */
  if (sim->config.debug.rsp_enabled)
    {
      rsp_init ();

      /* RSP always starts stalled as though we have just reset the
	 processor. */
      rsp_exception (EXCEPT_TRAP);
      set_stall_state (sim, 1);
    }
  else if (sim->config.debug.gdb_enabled)
    {
      gdbcomm_init (sim);
    }

  /* Enable dependency stats, if we want to do history analisis */
  if (sim->config.sim.history && !sim->config.cpu.dependstats)
    {
      sim->config.cpu.dependstats = 1;
      if (sim->config.sim.verbose)
	{
	  fprintf (stderr, "WARNING: sim_init: dependstats stats must be "
		   "enabled to do history analysis\n");
	}
    }

  /* Debug forces verbose */
  if (sim->config.sim.debug && !sim->config.sim.verbose)
    {
      sim->config.sim.verbose = 1;
      fprintf (stderr, "WARNING: sim_init: verbose forced on by debug\n");
    }

  /* Start VAPI before device initialization.  */
  if (sim->config.vapi.enabled)
    {
      sim->runtime.vapi.enabled = 1;
      vapi_init (sim);
      if (sim->config.sim.verbose)
	{
	  PRINTF ("VAPI started, waiting for clients\n");
	}
    }

  /* Wait till all test are connected.  */
  if (sim->runtime.vapi.enabled)
    {
      int numu = vapi_num_unconnected (sim, 0);
      if (numu)
	{
	  PRINTF ("\nWaiting for VAPI tests with ids:\n");
	  vapi_num_unconnected (sim, 1);
	  PRINTF ("\n");
	  while ((numu = vapi_num_unconnected (sim, 0)))
	    {
	      vapi_check ();
	      PRINTF
		("\rStill waiting for %i VAPI test(s) to connect",
		 numu);
	      usleep (100);
	    }
	  PRINTF ("\n");
	}
      PRINTF ("All devices connected\n");
    }
}	/* sim_init() */


/*---------------------------------------------------------------------------*/
/*!Clean up

   Close an profile or log files, disconnect VAPI. Call any memory mapped
   peripheral close down function. Exit with rc 0.                           */
/*---------------------------------------------------------------------------*/
void
sim_done (or1ksim *sim)
{
  if (sim->config.sim.profile)
    {
      fprintf (sim->runtime.sim.fprof, "-%08llX FFFFFFFF\n", sim->runtime.sim.cycles);
      fclose (sim->runtime.sim.fprof);
    }

  if (sim->config.sim.mprofile)
    {
      fclose (sim->runtime.sim.fmprof);
    }

  if (sim->config.sim.exe_log)
    {
      fclose (sim->runtime.sim.fexe_log);
    }

  if (sim->runtime.vapi.enabled)
    {
      vapi_done (sim);
    }

  done_memory_table (sim);
  mc_done (sim);

  exit (0);

}	/* sim_done() */
