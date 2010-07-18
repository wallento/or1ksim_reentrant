/* siminstance.h -- Simulator configuration header file

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
   Copyright (C) 2008 Embecosm Limited

   Contributors Jeremy Bennett <jeremy.bennett@embecosm.com>
                Stefan Wallentowitz, stefan.wallentowitz@tum.de
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

#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>

#include "defines.h"
//#include "cpu/or1k/spr-defs.h"

// from: sim-config.h
/*! Data structure for configuration data */
struct config
{
  struct
  {				/* External linkage for SystemC */
    void *class_ptr;
    unsigned long int (*read_up) (void *class_ptr,
				  unsigned long int addr,
				  unsigned long int mask);
    void (*write_up) (void *class_ptr,
		      unsigned long int addr,
		      unsigned long int mask, unsigned long int wdata);
  } ext;

  struct
  {
    int debug;			/* Simulator debugging */
    int verbose;		/* Force verbose output */

    int profile;		/* Is profiler running */
    char *prof_fn;		/* Profiler filename */

    int mprofile;		/* Is memory profiler running */
    char *mprof_fn;		/* Memory profiler filename */

    int history;		/* instruction stream history analysis */
    int exe_log;		/* Print out RTL states? */
    int exe_log_type;		/* Type of log */
    long long int exe_log_start;	/* First instruction to log */
    long long int exe_log_end;	/* Last instr to log, -1 if continuous */
    int exe_log_marker;		/* If nonzero, place markers before */
    /* each exe_log_marker instructions */
    char *exe_log_fn;		/* RTL state comparison filename */
    long clkcycle_ps;		/* Clock duration in ps */
    int strict_npc;		/* JPB. NPC flushes pipeline when changed */
  } sim;

  struct
  {				/* Verification API */
    int enabled;		/* Whether is VAPI module enabled */
    int server_port;		/* user specified port for services */
    int log_enabled;		/* Whether to log the vapi requests */
    int hide_device_id;		/* Whether to log dev ID each request */
    char *vapi_fn;		/* vapi log filename */
  } vapi;

  struct
  {
    char *timings_fn;		/* Filename of the timing table */
    int memory_order;		/* Memory access stricness */
    int calling_convention;	/* Do funcs follow std calling conv? */
    int enable_bursts;		/* Whether burst are enabled */
    int no_multicycle;		/* Generate no multicycle paths */
  } cuc;

  struct
  {
    int superscalar;		/* superscalara analysis */
    int hazards;		/* dependency hazards analysis */
    int dependstats;		/* dependency statistics */
    int sbuf_len;		/* length of store buffer, 0=disabled */
  } cpu;

  struct
  {
    int enabled;		/* Whether data cache is enabled */
    int nways;			/* Number of DC ways */
    int nsets;			/* Number of DC sets */
    int blocksize;		/* DC entry size */
    int ustates;		/* number of DC usage states */
    int store_missdelay;	/* cycles a store miss costs */
    int store_hitdelay;		/* cycles a store hit costs */
    int load_missdelay;		/* cycles a load miss costs */
    int load_hitdelay;		/* cycles a load hit costs */
  } dc;

  struct pic
  {
    int enabled;		/* Is interrupt controller enabled? */
    int edge_trigger;		/* Are interrupts edge triggered? */
  } pic;

  struct
  {
    int enabled;		/* Is power management operational? */
  } pm;

  struct
  {
    int enabled;		/* branch prediction buffer analysis */
    int sbp_bnf_fwd;		/* Static BP for l.bnf uses fwd predn */
    int sbp_bf_fwd;		/* Static BP for l.bf uses fwd predn */
    int btic;			/* BP target insn cache analysis */
    int missdelay;		/* How much cycles does the miss cost */
    int hitdelay;		/* How much cycles does the hit cost */
  } bpb;

  struct
  {
    int enabled;		/* Is debug module enabled */
    int gdb_enabled;		/* Is legacy debugging with GDB possible */
    int rsp_enabled;		/* Is RSP debugging with GDB possible */
    int server_port;		/* Port for legacy GDB connection */
    int rsp_port;		/* Port for RSP GDB connection */
    unsigned long vapi_id;	/* "Fake" vapi dev id for JTAG proxy */
  } debug;
};

/*! Data structure for run time data */
struct runtime
{
  struct
  {
    FILE *fprof;		/* Profiler file */
    FILE *fmprof;		/* Memory profiler file */
    FILE *fexe_log;		/* RTL state comparison file */
    FILE *fout;			/* file for standard output */
    char *filename;		/* Original Command Simulator file (CZ) */
    int iprompt;		/* Interactive prompt */
    int iprompt_run;		/* Interactive prompt is running */
    long long cycles;		/* Cycles counts fetch stages */
    long long int end_cycles;	/* JPB. Cycles to end of quantum */
    double time_point;		/* JPB. Time point in the simulation */
    unsigned long int ext_int_set;	/* JPB. External interrupts to set */
    unsigned long int ext_int_clr;	/* DXL. External interrupts ti clear */

    int mem_cycles;		/* Each cycle has counter of mem_cycles;
				   this value is joined with cycles
				   at the end of the cycle; no sim
				   originated memory accesses should be
				   performed inbetween. */
    int loadcycles;		/* Load and store stalls */
    int storecycles;

    long long reset_cycles;

    int  hush;			/* Is simulator to do reg dumps */
  } sim;

  struct
  {
    long long instructions;	/* Instructions executed */
    long long reset_instructions;

    int stalled;
    int hazardwait;		/* how many cycles were wasted because of hazards */
    int supercycles;		/* Superscalar cycles */
  } cpu;

  struct
  {				/* Verification API, part of Advanced Core Verification */
    int enabled;		/* Whether is VAPI module enabled */
    FILE *vapi_file;		/* vapi file */
    int server_port;		/* A user specified port number for services */
  } vapi;

/* CUC configuration parameters */
  struct
  {
    int mdelay[4];		/* average memory delays in cycles
				   {read single, read burst, write single, write burst} */
    double cycle_duration;	/* in ns */
  } cuc;
};

/* Generic structure for a configuration section */
struct config_section
{
  char *name;
  void *(*sec_start) (or1ksim *sim);
  void (*sec_end) (or1ksim *sim, void *);
  void *dat;
  struct config_param *params;
  struct config_section *next;
};

// from: arch.h
/* Basic types for openrisc */
typedef uint32_t  oraddr_t;	/*!< Address as addressed by openrisc */
typedef uint32_t  uorreg_t;	/*!< An unsigned register of openrisc */
typedef int32_t   orreg_t;	/*!< A signed register of openrisc */

// from: abstract.h
/*! Instruction queue */
struct iqueue_entry
{
  int       insn_index;
  uint32_t  insn;
  oraddr_t  insn_addr;
};

// from: execute.h
/*!The main structure holding the current execution state of the CPU

   Not to be confused with @c runtime, which holds the state of the
   simulation.

   @c insn_ea field is only used to get dump_exe_log() correct.

   @c iqueue and @c icomplet fields are only used in analysis().

   The micro-operation queue, @c opqs, is only used to speed up
   recompile_page().                                                         */
struct cpu_state {
  uorreg_t             reg[MAX_GPRS];	/*!< General purpose registers */
  uorreg_t             sprs[MAX_SPRS];	/*!< Special purpose registers */
  oraddr_t             insn_ea;		/*!< EA of instrs that have an EA */
  int                  delay_insn;	/*!< Is current instr in delay slot */
  int                  npc_not_valid;	/*!< NPC updated while stalled */
  oraddr_t             pc;		/*!< PC (and translated PC) */
  oraddr_t             pc_delay;	/*!< Delay instr EA register */
  uint32_t             pic_lines;	/*!< State of PIC lines */
  struct iqueue_entry  iqueue;		/*!< Decode of just executed instr */
  struct iqueue_entry  icomplet;        /*!< Decode of instr before this */

#if DYNAMIC_EXECUTION
  jmp_buf              excpt_loc;	/*!< Longjump here for exception */
  struct dyn_page     *curr_page;	/*!< Current page in execution */
  struct dyn_page    **dyn_pages;	/*!< Pointers to recompiled pages */
  int32_t              cycles_dec;
  struct op_queue     *opqs;		/*!< Micro-operation queue */
#endif
};

// from: or32.h
enum insn_type {
 it_unknown,
 it_exception,
 it_arith,
 it_shift,
 it_compare,
 it_branch,
 it_jump,
 it_load,
 it_store,
 it_movimm,
 it_move,
 it_extend,
 it_nop,
 it_mac,
 it_float };

// from: stats.h
struct bpbstat
{
  int hit;
  int miss;
  int correct;
  int incorrect;
};

struct bticstat
{
  int hit;
  int miss;
};

struct mstats_entry
{
  int byteadd;
  int bf[2][2];			/* [taken][fwd/bwd] */
  int bnf[2][2];		/* [taken][fwd/bwd] */
  struct bpbstat bpb;
  struct bticstat btic;
};				/*!< misc units stats */

struct cachestats_entry
{
  int readhit;
  int readmiss;
  int writehit;
  int writemiss;
};				/*!< cache stats */

struct immustats_entry
{
  int fetch_tlbhit;
  int fetch_tlbmiss;
  int fetch_pagefaults;
};				/*!< IMMU stats */

struct dmmustats_entry
{
  int loads_tlbhit;
  int loads_tlbmiss;
  int loads_pagefaults;
  int stores_tlbhit;
  int stores_tlbmiss;
  int stores_pagefaults;
};				/*!< DMMU stats */

struct raw_stats
{
  int reg[64];
  int range[RAW_RANGE];
};				/*!< RAW hazard stats */

// from: stats.c
struct dstats_entry
{
  int insn1;
  int insn2;
  int cnt_dynamic;
  int depend;
};				/*!< double stats */

struct sstats_entry
{
  int insn;
  int cnt_dynamic;
};				/*!< single stats */

struct fstats_entry
{
  enum insn_type insn1;
  enum insn_type insn2;
  int cnt_dynamic;
  int depend;
};				/*!< functional units stats */

// from: cuc.h
/* Instructionn entity */
typedef struct
{
  int type;			/* type of the instruction */
  int index;			/* Instruction index */
  int opt[MAX_OPERANDS];	/* operand types */
  unsigned long op[MAX_OPERANDS];	/* operand values */
  struct _dep_list_t *dep; 	/* instruction dependencies */
  unsigned long insn;		/* Instruction opcode */
  char disasm[40];		/* disassembled string */
  unsigned long max;		/* max result value */
  int tmp;
} cuc_insn;

/*! The various addresses in the development interface scan chain
    (JTAG_CHAIN_DEVELOPMENT). Only documents the ones we actually have*/
enum development_interface_address_space
{
  DEVELOPINT_RISCOP  =  4,
  DEVELOPINT_MAX     = 27,
};

// from: debug-unit.h
/*! Enumeration of the various JTAG scan chains. Only those actually
    implemented are specified. */
enum debug_scan_chain_ids
{
  JTAG_CHAIN_GLOBAL      = 0,
  JTAG_CHAIN_DEBUG_UNIT  = 1,
  JTAG_CHAIN_TRACE       = 3,
  JTAG_CHAIN_DEVELOPMENT = 4,
  JTAG_CHAIN_WISHBONE    = 5,
};

// from: rsp-serber.c
#define MP_HASH_SIZE  1021 /*!< Size of the matchpoint hash table. Largest prime < 2^10 */

// from: channel.c
struct channel_factory
{
  const char *name;
  const struct channel_ops *ops;
  struct channel_factory *next;
};

// from: sched.h
/*! Structure for holding one job entry */
struct sched_entry
{
  int32_t time;			/* Clock cycles before job starts */
  void *param;			/* Parameter to pass to the function */
  void (*func) (or1ksim *sim, void *);	/* Function to call when time reaches 0 */
  struct sched_entry *next;
};

/*! Heap of jobs */
struct scheduler_struct
{
  struct sched_entry *free_job_queue;
  struct sched_entry *job_queue;
};

// from: vapi.c
struct vapi_handler
{
	int fd;
	unsigned long base_id, num_ids;
	void (*read_func) (or1ksim *sim,unsigned long, unsigned long, void *);
	void *priv_dat;
	struct vapi_handler *next;
	int temp;
};

// from: profiler.h
/*! Data structure for information about functions */
struct func_struct {
  unsigned int  addr;		/*!< Start address of function */
  char          name[33];	/*!< Name of the function */
  long          cum_cycles;	/*!< Total cycles spent in function */
  long          calls;		/*!< Calls to this function */
};

// from: profiler.c
/*! Data structure representing information about a stack frame */
struct stack_struct
{
  unsigned int  addr;	   /*!< Function address */
  unsigned int  cycles;    /*!< Cycles of func start; subfuncs added later */
  unsigned int  raddr;	   /*!< Return address */
  char          name[33];  /*!< Name of the function */
};

// from: mprofiler.c
/*! Hash table data structure */
struct memory_hash
{
  struct memory_hash *next;
  oraddr_t            addr;
  unsigned long       cnt[3];		/* Various counters */
};

#endif /* TYPES_H_ */
