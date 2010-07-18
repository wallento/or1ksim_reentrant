/* sched.h -- Abstract entities header file handling job scheduler

   Copyright (C) 2001 Marko Mlinar, markom@opencores.org
   Copyright (C) 2005 György `nog' Jeney, nog@sdf.lonestar.org
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


#ifndef _SCHED_H_
#define _SCHED_H_

#include "siminstance.h"

/*! Macro to add a job to the scheduler */
#define SCHED_ADD(job_func, job_param, job_time) sched_add(sim, job_func, job_param, job_time, #job_func)

/*! Macro to remove a job from the scheduler */
#define SCHED_FIND_REMOVE(f, p) sched_find_remove(sim, f, p)

/* Function prototypes for external use */
extern void sched_init (or1ksim *sim);
extern void sched_reset (or1ksim *sim);
extern void sched_next_insn (or1ksim *sim, void  (*func) (or1ksim *sim, void *),
			     void *dat);
extern void sched_find_remove (or1ksim *sim, void        (*job_func) (or1ksim *sim, void *),
			       void       *dat);
extern void sched_add (or1ksim *sim, void        (*job_func) (or1ksim *sim, void *),
		       void       *job_param,
		       int32_t     job_time,
		       const char *func);
extern void do_scheduler (or1ksim *sim);

#endif /* _SCHED_H_ */
