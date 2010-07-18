/* siminstance.h -- Simulator configuration header file

   Copyright (C) 2009 Stefan Wallentowitz, stefan.wallentowitz@tum.de

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

#ifndef SIMINSTANCE_H_
#define SIMINSTANCE_H_

#include <stdio.h>

/*!
 * The type to access the simulator instance struct or1ksim.
 */
typedef struct or1ksim or1ksim; // Unfortunately required as forward declaration

#include "types.h"
#include "defines.h"

/*!Simulator Instance
 *
 * All configuration and status data of the simulator has been encapsulated in this
 * structure to allow multiple instances of the simulator running in parallel, e.g.,
 * as library instances in a parallel simulation.
 *
 * The global variables have been moved here. That is a very UGLY thing, perhaps this
 * could be modularized and the final goal should be a C++ object oriented implemented.
 *
 * There is no global instance of the simulation, but they have to be created manually.
 * Malloc your instance and set it up with sim_defaults.
 */
struct or1ksim {
	struct config  config;
	struct config_section *cur_section;
	struct config_section *sections;

	struct runtime runtime;

	/*
	 *  execute state and information
	 */
	struct cpu_state  cpu_state; /*!< Current cpu state. */
	oraddr_t  pcnext; /*!< Temporary program counter. */
	int  sbuf_wait_cyc; /*!< Num cycles waiting for stores to complete. */
	int  sbuf_total_cyc; /*!< Number of total store cycles. */
	int  do_stats; /*!< Whether we are doing statistical analysis. */
	struct hist_exec *hist_exec_tail; /*!< History of execution. */
	int  multissue[20]; /*!< Benchmark multi issue execution. */
	int  issued_per_cycle;
	/* Store buffer analysis - stores are accumulated and commited when IO is idle. */
	int  sbuf_head;
	int  sbuf_tail;
	int  sbuf_count;
	#if !(DYNAMIC_EXECUTION)
	int  sbuf_buf[MAX_SBUF_LEN];
	#endif
	int sbuf_prev_cycles;

	/* Variables used throughout this file to share information */
	int  breakpoint;
	int  next_delay_insn;

	/*
	 * Reset hooks
	 */
	struct sim_reset_hook *sim_reset_hooks; /*!< The list of reset hooks. Local to this source file */

	/*
	 * Statistics data. Renamed mstats to or1k_mstats because Mac OS X has a lib function called mstats
	 */
	struct mstats_entry      or1k_mstats;	/*!< misc units stats */
	struct cachestats_entry  ic_stats;	/*!< instruction cache stats */
	struct cachestats_entry  dc_stats;	/*!< data cache stats */
	struct immustats_entry   immu_stats;	/*!< insn MMU stats */
	struct dmmustats_entry   dmmu_stats;	/*!< data MMU stats */
	struct raw_stats         raw_stats;		/*!< RAW hazard stats */

	/* Statistics data strutures used just here */
	struct dstats_entry  dstats[DSTATS_LEN];	/*!< dependency stats */
	struct sstats_entry  sstats[SSTATS_LEN];	/*!< single stats */
	struct fstats_entry  fstats[FSTATS_LEN];	/*!< func units stats */

	/*
	 * Labels
	 */
	struct breakpoint_entry *breakpoints; /*!< List of breakpoints */
	struct label_entry *label_hash[LABELS_HASH_SIZE]; /*!< List of labels (symbols) */

	/*
	 * Memory
	 */
	struct dev_memarea *cur_area; /*!< Temporary variable (originally intended global to increase speed).  */

	/* Set by MMU if cache inhibit bit is set for current access.  */
	int  data_ci;			/*!< Global var: data cache inhibit bit set */
	int  insn_ci;			/*!< Global var: instr cache inhibit bit set */

	/* Pointer to memory area descriptions that are assigned to individual peripheral devices. */
	struct dev_memarea *dev_list;

	/* Pointer to memory controller device descriptor.  */
	struct dev_memarea *mc_area;

	/* Virtual address of current access. */
	oraddr_t cur_vadd;

	/*
	 * Translation/Parser
	 */
	unsigned int  freemem; 	/*!< Unused mem memory marker. It is used when allocating program and data memory during parsing */
	oraddr_t  transl_table; /*!Translation table provided by microkernel. Only used if simulating microkernel. */
	uint32_t  transl_error; 	/*!Used to signal whether during loading of programs a translation fault  occured. */

	#if IMM_STATS
	int       bcnt[33][3];
	int       bsum[3];
	uint32_t  movhi;
	#endif  /* IMM_STATS */

	// from: or32
	int range_cache[256]; /* Simple cache for letter ranges */

	/*
	 * Branch Prediction
	 */
	struct bpb_entry
	{
	  struct
	  {
	    oraddr_t addr;		/* address of a branch insn */
	    int taken;			/* taken == 1, not taken == 0  OR */
	    /* strongly taken == 3, taken == 2,
	       not taken == 1, strongly not taken == 0 */
	    int lru;			/* least recently == 0 */
	  } way[BPB_WAYS];
	} bpb[BPB_LEN]; /*!< branch prediction buffer entry */

	/*
	 *  Data cache
	 */
	struct dc_set
	{
	  struct
	  {
	    uint32_t line[MAX_DC_BLOCK_SIZE/4];
	    oraddr_t tagaddr;		/* tag address */
	    int lru;			/* least recently used */
	  } way[MAX_DC_WAYS];
	} dc[MAX_DC_SETS];

	/*
	 * Instruction cache
	 */
	struct ic *ic_state;

	// from: or32.c, wallento: This might only be necessary for generate
	unsigned long *automata;
	int nuncovered;
	int curpass;

	/* MM: Struct that holds runtime build information about instructions.  */
	struct temp_insn_struct *ti;

	struct insn_op_struct *op_data, **op_start;
	char disassembled_str[50];
	char *disassembled;

	/*
	 * CUC
	 */
	FILE *flog;
	int cuc_debug;

	struct _cuc_func *func[MAX_FUNCS];
	int func_v[MAX_FUNCS];

	/* Instructions from function */
	cuc_insn insn[MAX_INSNS];
	int num_insn;
	int reloc[MAX_INSNS];

	/* First primary input */
	unsigned long tmp_op, tmp_opt;
	struct _csm_list *main_list;
	int *iteration;
	struct _cuc_timing_table *timing_table;
	double max_bb_delay;

	/*
	 * Debug
	 */
	/*! Data structure holding debug registers and their bits */
	unsigned long  development[DEVELOPINT_MAX + 1];

	/*! The current scan chain being accessed */
	enum debug_scan_chain_ids current_scan_chain;

	/*! External STALL signal to debug interface */
	int in_reset;

	unsigned int  server_ip;
	unsigned int  server_port;
	unsigned int  server_fd;
	unsigned int  gdb_fd;
	int           tcp_level;

	/*! Central data for the RSP connection */
	struct
	{
	  int                client_waiting;	/*!< Is client waiting a response? */
	  int                proto_num;		/*!< Number of the protocol used */
	  int                client_fd;		/*!< FD for talking to GDB */
	  int                sigval;		/*!< GDB signal for any exception */
	  unsigned long int  start_addr;	/*!< Start of last run */
	  struct mp_entry   *mp_hash[MP_HASH_SIZE];	/*!< Matchpoint hash table */
	} rsp;

	/*
	 * MMU
	 */
	struct dmmu *dmmu_state;
	struct immu *immu_state;

	/*
	 * Channels
	 */
	struct channel_factory preloaded[5];
	struct channel_factory *head;

	/*
	 * Peripheral
	 */
	unsigned int conf_dev; /*!< ATA Configuration */

	/* We keep a copy of all our controllers because we have to export an interface
	 * to other peripherals eg. ethernet */
	struct dma_controller *dmas;

	struct mc *mcs;
	/* List used to temporarily hold memory areas registered with the mc, while the
	 * mc configureation has not been loaded */
	struct mc_area *mc_areas;

	/*
	 * PIC
	 */

	/* FIXME: This ugly hack will be removed once the bus architecture gets written
	 */
	// wallento: This is not used as far as I see
//	struct pic pic_state_int;
//	struct pic *pic_state;

	/*
	 * Debug
	 */
#define DECLARE_DEBUG_CHANNEL(dbch) char* __orsim_dbch_##dbch;
#include "support/dbchs.h"
#undef DECLARE_DEBUG_CHANNEL

	char **__orsim_dbchs;

	/*
	 * Scheduler
	 */

	struct scheduler_struct scheduler; /*!< FIXME: Scheduler should continue from previous cycles not current ones */

	/*
	 * Simprintf
	 */
	char fmtstr[FMTLEN];

	/*
	 * Tick
	 */
	long long cycles_start; /*!< When did the timer start to count */
	int tick_count;	/*!< Indicates if the timer is actually counting.  Needed to simulate one-shot mode correctly */

	/*
	 * Vapi
	 */
	struct {
		unsigned int serverIP;
		unsigned int server_fd;
		unsigned int nhandlers;

		int tcp_level;

		struct vapi_handler *vapi_handler;

		/* Structure for polling, it is cached, that it doesn't have to be rebuilt each time */
		struct pollfd *fds;
		int nfds;
	} vapi;

	int except_pending;

	/*
	 * SPR
	 */
	char ret_spr[1000]; /*!< Should be long enough for everything */
	int audio_cnt;
	FILE *fo;

	struct {
		struct func_struct  prof_func[MAX_FUNCS]; /*!< data about functions */
		int  prof_nfuncs;                         /*!< total number of functions */
		int  prof_cycles;                         /*!< current cycles */
		struct stack_struct  stack[MAX_STACK];    /*!< Representation of the stack */
		int  nstack;                              /*!< Current depth */
		int  maxstack;                            /*!< Max depth */
		int  ntotcalls;                           /*!< Number of total calls */
		int  nfunccalls;                          /*!< Number of covered calls */
		int  cumulative;                          /*!< Whether we are in cumulative mode */
		int  quiet;                               /*!< Whether we should not report warnings */
		FILE *fprof;                              /*!< File to read from */
	} profiler;

	struct {
		struct memory_hash *hash[HASH_SIZE]; /*!< Hash table data structure */
		int  group_bits;                     /*!< Groups size -- how much addresses should be joined together */
		oraddr_t  start_addr;                /*!< Start address */
		oraddr_t  end_addr;                  /*!< End address */
		FILE *fprof;                         /*!< File to read from */
	} mprofiler;

	struct sim_stat *sim_stats;
	long long to_insn_num; /*!< The number of instructions to execute before droping into interactive mode */

	struct btic_entry
	{
	  struct
	  {
	    oraddr_t addr;		/* cached target address of a branch */
	    int lru;			/* least recently used */
	    char *insn;			/* cached insn at target address (not used currently) */
	  } way[BTIC_WAYS];
	} btic[BTIC_LEN]; /*!< Branch target instruction cache */
};

#endif /* SIMINSTANCE_H_ */
