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

#ifndef DEFINES_H_
#define DEFINES_H_

// from sim-config.h
/* Simulator configuration macros. Eventually this one will be a lot bigger. */

#define MAX_SBUF_LEN     256	/* Max. length of store buffer */

#define EXE_LOG_HARDWARE   0	/* Print out RTL states */
#define EXE_LOG_SIMPLE     1	/* Executed log prints out dissasembly */
#define EXE_LOG_SOFTWARE   2	/* Simple with some register output */

#define STR_SIZE         256

/* Number of cycles between checks to runtime.sim.iprompt */
#define CHECK_INT_TIME 100000

#define PRINTF(x...) fprintf (sim->runtime.sim.fout, x)

// from: or32.h
#define MAX_GPRS 32

// from: spr-defs.h
#define MAX_SPRS (0x10000)

// from: stats.c
#define DSTATS_LEN	3000
#define SSTATS_LEN	300
#define FSTATS_LEN	200

// from: stats.h
#define RAW_RANGE  1000

// from: labels.c
#define LABELS_HASH_SIZE 119

// from: parse.c
#define IMM_STATS 0 /*!< Whether to do immediate statistics. This seems to be for local debugging of parse.c */

// from: branch-predict.c
#define BPB_LEN 64 /*!< Length of BPB */
#define BPB_WAYS 1 /*!< Number of BPB ways (1, 2, 3 etc.). */

// from: dcache-model.h
#define MAX_DC_SETS        1024
#define MAX_DC_WAYS          32
#define MIN_DC_BLOCK_SIZE    16
#define MAX_DC_BLOCK_SIZE    32

// from: profiler.h
#define MAX_FUNCS 1024 /*!< Maximum number of functions that can be profiled */

// from: profiler.c
#define MAX_STACK  1024 /*!< Maximum stack frames that can be profiled */

// from: mprofiler.c
#define HASH_SIZE       0x10000

//from: cuc.h
#define MAX_INSNS	0x10000 /*!< Maximum number of instructions per function */

//from: abstract.h
#define MAX_OPERANDS                 5

//from: simprintf.c
#define FMTLEN 2000	/*!< Length of PRINTF format string */

//from: branch-predict.c
#define BTIC_LEN 128 /*!< Length of BTIC */
#define BTIC_WAYS 2 /*!< Number of BTIC ways (1, 2, 3 etc.). */

#endif /* DEFINES_H_ */
