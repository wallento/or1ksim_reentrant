/* load.c -- OpenRISC Custom Unit Compiler, instruction loading and converting
 *    Copyright (C) 2002 Marko Mlinar, markom@opencores.org
 *    Copyright (C) 2009 Stefan Wallentowitz, stefan.wallentowitz@tum.de
 *
 *    This file is part of OpenRISC 1000 Architectural Simulator.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "config.h"

#include "port.h"
#include "arch.h"
#include "abstract.h"
#include "sim-config.h"
#include "cuc.h"
#include "opcode/or32.h"
#include "insn.h"
#include "siminstance.h"

static const cuc_conv conv[] = {
{"l.add", II_ADD}, {"l.addi", II_ADD},
{"l.movhi", II_OR},
{"l.sub", II_SUB}, {"l.subi", II_SUB},
{"l.and", II_AND}, {"l.andi", II_AND},
{"l.xor", II_XOR}, {"l.xori", II_XOR},
{"l.or",  II_OR }, {"l.ori",  II_OR},
{"l.mul", II_MUL}, {"l.muli", II_MUL},

{"l.sra", II_SRA}, {"l.srai", II_SRA},
{"l.srl", II_SRL}, {"l.srli", II_SRL},
{"l.sll", II_SLL}, {"l.slli", II_SLL},

{"l.lbz",II_LB | II_MEM}, {"l.lbs", II_LB | II_MEM | II_SIGNED},
{"l.lhz",II_LH | II_MEM}, {"l.lhs", II_LH | II_MEM | II_SIGNED},
{"l.lwz",II_LW | II_MEM}, {"l.lws", II_LW | II_MEM | II_SIGNED},
{"l.sb", II_SB | II_MEM}, {"l.sh",  II_SH | II_MEM}, {"l.sw", II_SW | II_MEM},
{"l.sfeq",  II_SFEQ }, {"l.sfeqi", II_SFEQ},
{"l.sfne",  II_SFNE }, {"l.sfnei", II_SFNE},
{"l.sflts", II_SFLT | II_SIGNED}, {"l.sfltis", II_SFLT | II_SIGNED},
{"l.sfltu", II_SFLT}, {"l.sfltiu", II_SFLT},
{"l.sfgts", II_SFGT | II_SIGNED}, {"l.sfgtis", II_SFGT | II_SIGNED},
{"l.sfgtu", II_SFGT}, {"l.sfgtiu", II_SFGT},
{"l.sfges", II_SFGE | II_SIGNED}, {"l.sfgeis", II_SFGE | II_SIGNED},
{"l.sfgeu", II_SFGE}, {"l.sfgeiu", II_SFGE},
{"l.sfles", II_SFLE | II_SIGNED}, {"l.sfleis", II_SFLE | II_SIGNED},
{"l.sfleu", II_SFLE}, {"l.sfleiu", II_SFLE},
{"l.j",     II_BF   },
{"l.bf",    II_BF   },
{"l.jal",   II_CALL },
{"l.nop",   II_NOP  }
};

/* Prints out instructions */
void print_cuc_insns (or1ksim *sim, char *s, int verbose)
{
  PRINTF ("****************** %s ******************\n", s);
  print_insns (sim, 0, sim->insn, sim->num_insn,verbose);
  PRINTF ("\n\n");
}

void xchg_insn (or1ksim* sim, int i, int j)
{
  cuc_insn t;
  t = sim->insn[i];
  sim->insn[i] = sim->insn[j];
  sim->insn[j] = t;
}

/* Negates conditional instruction */
void negate_conditional (cuc_insn *ii)
{
  assert (ii->type & IT_COND);

  if (ii->index == II_SFEQ) change_insn_type (ii, II_SFNE);
  else if (ii->index == II_SFNE) change_insn_type (ii, II_SFEQ);
  else if (ii->index == II_SFLT) change_insn_type (ii, II_SFGE);
  else if (ii->index == II_SFGT) change_insn_type (ii, II_SFLE);
  else if (ii->index == II_SFLE) change_insn_type (ii, II_SFGT);
  else if (ii->index == II_SFGE) change_insn_type (ii, II_SFLT);
  else assert (0);
}

/* Remove delay slots */
void remove_dslots (or1ksim* sim)
{
  int i;
  int in_delay = 0;
  for (i = 0; i < sim->num_insn; i++) {
    if (in_delay) sim->insn[i].type |= IT_INDELAY;
    in_delay = 0;
    if (sim->insn[i].type & IT_BRANCH) in_delay = 1;
    if (sim->insn[i].type & IT_INDELAY) {
      cuc_insn *ii;
      cuc_insn *bi;
      assert (i >= 2);
      ii = &sim->insn[i - 2];
      bi = &sim->insn[i - 1];
      /* delay slot should not be a branch target! */
      assert ((sim->insn[i].type & IT_BBSTART) == 0);
      assert ((bi->type & IT_INDELAY) == 0);
      sim->insn[i].type &= ~IT_INDELAY; /* no more in delay slot */

      /* Get the value we need before the actual jump */
      if (bi->opt[1] & OPT_REGISTER && bi->op[1] >= 0) {
        int r = bi->op[1];
        assert (ii->index == II_NOP);
        change_insn_type (ii, II_ADD);
        ii->type = IT_COND;
        ii->dep = NULL;
        ii->op[0] = r; ii->opt[0] = OPT_REGISTER | OPT_DEST;
        ii->op[1] = r; ii->opt[1] = OPT_REGISTER;
        ii->op[2] = 0; ii->opt[2] = OPT_CONST;
        ii->opt[3] = OPT_NONE;
        bi->op[1] = i - 2; bi->opt[1] = OPT_REF;
      }
      xchg_insn (sim, i, i - 1);
    }
  }
  assert (in_delay == 0);
}

/* Convert local variables (uses stack frame -- r1) to internal values */
void detect_locals (or1ksim* sim)
{
  int stack[CUC_MAX_STACK];
  int i, can_remove_stack = 1;
  int real_stack_size = 0;

  for (i = 0; i < CUC_MAX_STACK; i++) stack[i] = -1;

  for (i = 0; i < sim->num_insn; i++) {
    /* sw off (r1),rx */
    if (sim->insn[i].index == II_SW
      && (sim->insn[i].opt[0] & OPT_CONST)
      && sim->insn[i].op[1] == 1 && (sim->insn[i].opt[1] & OPT_REGISTER)) {

      if (sim->insn[i].op[0] < CUC_MAX_STACK/* && sim->insn[i].op[1] >= 4*/) { /* Convert to normal move */
        stack[sim->insn[i].op[0]] = i;
        sim->insn[i].type &= IT_INDELAY | IT_BBSTART;
        change_insn_type (&sim->insn[i], II_ADD);
        sim->insn[i].op[0] = -1; sim->insn[i].opt[0] = OPT_REGISTER | OPT_DEST;
        sim->insn[i].op[1] = sim->insn[i].op[2]; sim->insn[i].opt[1] = sim->insn[i].opt[2];
        sim->insn[i].op[2] = 0; sim->insn[i].opt[2] = OPT_CONST;
      } else can_remove_stack = 0;
    /* lw rx,off (r1) */
    } else if (sim->insn[i].index == II_LW
      && (sim->insn[i].opt[1] & OPT_CONST)
      && sim->insn[i].op[2] == 1 && (sim->insn[i].opt[2] & OPT_REGISTER)) {

      if (sim->insn[i].op[1] < CUC_MAX_STACK && stack[sim->insn[i].op[1]] >= 0) { /* Convert to normal move */
        sim->insn[i].type &= IT_INDELAY | IT_BBSTART;
        change_insn_type (&sim->insn[i], II_ADD);
        sim->insn[i].op[1] = stack[sim->insn[i].op[1]]; sim->insn[i].opt[1] = OPT_REF;
        sim->insn[i].op[2] = 0; sim->insn[i].opt[2] = OPT_CONST;
      } else can_remove_stack = 0;
    /* Check for defined stack size */
    } else if (sim->insn[i].index == II_ADD && !real_stack_size
            && (sim->insn[i].opt[0] & OPT_REGISTER) && sim->insn[i].op[0] == 1
            && (sim->insn[i].opt[1] & OPT_REGISTER) && sim->insn[i].op[1] == 1
            && (sim->insn[i].opt[2] & OPT_CONST)) {
      real_stack_size = -sim->insn[i].op[2];
    }
  }
  //assert (can_remove_stack); /* TODO */
}

/* Disassemble one instruction from sim->insn index and generate parameters */
const char *build_insn (or1ksim *sim, unsigned long data, cuc_insn *insn)
{
  const char *name;
  char *s;
  int index = insn_decode (sim, data);
  struct or32_opcode const *opcode;
  int i, argc = 0;

  sim->insn->insn = data;
  sim->insn->index = -1;
  sim->insn->type = 0;
  name = insn_name (index);
  sim->insn->index = index;
  disassemble_index (sim, data, index);
  strcpy (sim->insn->disasm, sim->disassembled);
  sim->insn->dep = NULL;
  for (i = 0; i < MAX_OPERANDS; i++) sim->insn->opt[i] = OPT_NONE;

  if (index < 0) {
    fprintf (stderr, "Invalid opcode 0x%08lx!\n", data);
    exit (1);
  }
  opcode = &or32_opcodes[index];

  for (s = opcode->args; *s != '\0'; ++s) {
    switch (*s) {
    case '\0': return name;
    case 'r':
      sim->insn->opt[argc] = OPT_REGISTER | (argc ? 0 : OPT_DEST);
      sim->insn->op[argc++] = or32_extract(*++s, opcode->encoding, data);
      break;

    default:
      if (strchr (opcode->encoding, *s)) {
        unsigned long imm = or32_extract (*s, opcode->encoding, data);
        imm = extend_imm(sim, imm, *s);
        sim->insn->opt[argc] = OPT_CONST;
        sim->insn->op[argc++] = imm;
      }
    }
  }
  return name;
}

/* inserts nop before branch */
void expand_branch (or1ksim* sim)
{
  int i, j, num_bra = 0, d;
  for (i = 0; i < sim->num_insn; i++) if (sim->insn[i].type & IT_BRANCH) num_bra++;

  d = sim->num_insn + 2 * num_bra;
  assert (d < MAX_INSNS);

  /* Add nop before branch */
  for (i = sim->num_insn - 1; i >= 0; i--) if (sim->insn[i].type & IT_BRANCH) {
    sim->insn[--d] = sim->insn[i]; // for delay slot (later)
    if (sim->insn[d].opt[1] & OPT_REGISTER) {
      assert (sim->insn[d].op[1] == FLAG_REG);
      sim->insn[d].op[1] = i; sim->insn[d].opt[1] = OPT_REF;
    }
    sim->insn[--d] = sim->insn[i]; // for branch
    change_insn_type (&sim->insn[d], II_NOP);
    sim->insn[--d] = sim->insn[i]; // save flag & negation of conditional, if required
    change_insn_type (&sim->insn[d], II_CMOV);
    sim->insn[d].op[0] = -1; sim->insn[d].opt[0] = OPT_REGISTER | OPT_DEST;
    sim->insn[d].op[1] = sim->insn[d].type & IT_FLAG1 ? 0 : 1; sim->insn[d].opt[1] = OPT_CONST;
    sim->insn[d].op[2] = sim->insn[d].type & IT_FLAG1 ? 1 : 0; sim->insn[d].opt[2] = OPT_CONST;
    sim->insn[d].op[3] = FLAG_REG; sim->insn[d].opt[3] = OPT_REGISTER;
    sim->insn[d].type = IT_COND;
    if (sim->insn[d].type)
    sim->reloc[i] = d;
  } else {
    sim->insn[--d] = sim->insn[i];
    sim->reloc[i] = d;
  }
  sim->num_insn += 2 * num_bra;
  for (i = 0; i < sim->num_insn; i++)
    for (j = 0; j < MAX_OPERANDS; j++)
      if (sim->insn[i].opt[j] & OPT_REF || sim->insn[i].opt[j] & OPT_JUMP)
        sim->insn[i].op[j] = sim->reloc[sim->insn[i].op[j]];
}

/* expands immediate memory instructions to two */
void expand_memory (or1ksim* sim)
{
  int i, j, num_mem = 0, d;
  for (i = 0; i < sim->num_insn; i++) if (sim->insn[i].type & IT_MEMORY) num_mem++;

  d = sim->num_insn + num_mem;
  assert (d < MAX_INSNS);

  /* Split memory commands */
  for (i = sim->num_insn - 1; i >= 0; i--) if (sim->insn[i].type & IT_MEMORY) {
    sim->insn[--d] = sim->insn[i];
    sim->insn[--d] = sim->insn[i];
    sim->reloc[i] = d;
    switch (sim->insn[d].index) {
    case II_SW:
    case II_SH:
    case II_SB:
              sim->insn[d + 1].op[1] = d; sim->insn[d + 1].opt[1] = OPT_REF; /* sw rx,(t($-1)) */
              sim->insn[d + 1].op[0] = sim->insn[i].op[2]; sim->insn[d + 1].opt[0] = sim->insn[d + 1].opt[2];
              sim->insn[d + 1].opt[2] = OPT_NONE;
              sim->insn[d + 1].type &= ~IT_BBSTART;
              sim->insn[d].op[2] = sim->insn[d].op[0]; sim->insn[d].opt[2] = sim->insn[d].opt[0];
              sim->insn[d].op[0] = -1; sim->insn[d].opt[0] = OPT_REGISTER | OPT_DEST; /* add rd, ra, rb */
              sim->insn[d].opt[3] = OPT_NONE;
              sim->insn[d].type &= IT_INDELAY | IT_BBSTART;
              sim->insn[d].type |= IT_MEMADD;
              change_insn_type (&sim->insn[d], II_ADD);
              break;
    case II_LW:
    case II_LH:
    case II_LB:
              sim->insn[d].op[0] = -1; sim->insn[d].opt[0] = OPT_REGISTER | OPT_DEST; /* add rd, ra, rb */
              sim->insn[d].type &= IT_INDELAY | IT_BBSTART;
              sim->insn[d].type |= IT_MEMADD;
              change_insn_type (&sim->insn[d], II_ADD);
              sim->insn[d + 1].op[1] = d; sim->insn[d + 1].opt[1] = OPT_REF; /* lw (t($-1)),rx */
              sim->insn[d + 1].opt[2] = OPT_NONE;
              sim->insn[d + 1].opt[3] = OPT_NONE;
              sim->insn[d + 1].type &= ~IT_BBSTART;
              break;
    default:  fprintf (stderr, "%4i, %4i: %s\n", i, d, cuc_insn_name (&sim->insn[d]));
              assert (0);
    }
  } else {
    sim->insn[--d] = sim->insn[i];
    sim->reloc[i] = d;
  }
  sim->num_insn += num_mem;
  for (i = 0; i < sim->num_insn; i++) if (!(sim->insn[i].type & IT_MEMORY))
    for (j = 0; j < MAX_OPERANDS; j++)
      if (sim->insn[i].opt[j] & OPT_REF || sim->insn[i].opt[j] & OPT_JUMP)
        sim->insn[i].op[j] = sim->reloc[sim->insn[i].op[j]];
}

/* expands signed comparisons to three instructions */
void expand_signed (or1ksim* sim)
{
  int i, j, num_sig = 0, d;
  for (i = 0; i < sim->num_insn; i++)
    if (sim->insn[i].type & IT_SIGNED && !(sim->insn[i].type & IT_MEMORY)) num_sig++;

  d = sim->num_insn + num_sig * 2;
  assert (d < MAX_INSNS);

  /* Split signed instructions */
  for (i = sim->num_insn - 1; i >= 0; i--)
    /* We will expand signed memory later */
    if (sim->insn[i].type & IT_SIGNED && !(sim->insn[i].type & IT_MEMORY)) {
      sim->insn[--d] = sim->insn[i];
      sim->insn[d].op[1] = d - 2; sim->insn[d].opt[1] = OPT_REF;
      sim->insn[d].op[2] = d - 1; sim->insn[d].opt[2] = OPT_REF;

      sim->insn[--d] = sim->insn[i];
      change_insn_type (&sim->insn[d], II_ADD);
      sim->insn[d].type = 0;
      sim->insn[d].op[0] = -1; sim->insn[d].opt[0] = OPT_REGISTER | OPT_DEST;
      sim->insn[d].op[1] = sim->insn[d].op[2]; sim->insn[d].opt[1] = sim->insn[d].opt[2];
      sim->insn[d].op[2] = 0x80000000; sim->insn[d].opt[2] = OPT_CONST;
      sim->insn[d].opt[3] = OPT_NONE;

      sim->insn[--d] = sim->insn[i];
      change_insn_type (&sim->insn[d], II_ADD);
      sim->insn[d].type = 0;
      sim->insn[d].op[0] = -1; sim->insn[d].opt[0] = OPT_REGISTER | OPT_DEST;
      sim->insn[d].op[1] = sim->insn[d].op[1]; sim->insn[d].opt[1] = sim->insn[d].opt[1];
      sim->insn[d].op[2] = 0x80000000; sim->insn[d].opt[2] = OPT_CONST;
      sim->insn[d].opt[3] = OPT_NONE;

      sim->reloc[i] = d;
    } else {
      sim->insn[--d] = sim->insn[i];
      sim->reloc[i] = d;
    }
  sim->num_insn += num_sig * 2;
  for (i = 0; i < sim->num_insn; i++) if (sim->insn[i].type & IT_MEMORY || !(sim->insn[i].type & IT_SIGNED)) {
    for (j = 0; j < MAX_OPERANDS; j++)
      if (sim->insn[i].opt[j] & OPT_REF || sim->insn[i].opt[j] & OPT_JUMP)
        sim->insn[i].op[j] = sim->reloc[sim->insn[i].op[j]];
  } else sim->insn[i].type &= ~IT_SIGNED;
}

/* expands calls to 7 instructions */
void expand_calls (or1ksim* sim)
{
  int i, j, num_call = 0, d;
  for (i = 0; i < sim->num_insn; i++)
    if (sim->insn[i].index == II_CALL) num_call++;

  d = sim->num_insn + num_call * 6; /* 6 parameters */
  assert (d < MAX_INSNS);

  /* Split call instructions */
  for (i = sim->num_insn - 1; i >= 0; i--)
    /* We will expand signed memory later */
    if (sim->insn[i].index == II_CALL) {
      sim->insn[--d] = sim->insn[i];
      sim->insn[d].op[0] = sim->insn[d].op[1]; sim->insn[d].opt[0] = OPT_CONST;
      sim->insn[d].opt[1] = OPT_NONE;
      sim->insn[d].type |= IT_VOLATILE;

      for (j = 0; j < 6; j++) {
        sim->insn[--d] = sim->insn[i];
        change_insn_type (&sim->insn[d], II_ADD);
        sim->insn[d].type = IT_VOLATILE;
        sim->insn[d].op[0] = 3 + j; sim->insn[d].opt[0] = OPT_REGISTER | OPT_DEST;
        sim->insn[d].op[1] = 3 + j; sim->insn[d].opt[1] = OPT_REGISTER;
        sim->insn[d].op[2] = 0x80000000; sim->insn[d].opt[2] = OPT_CONST;
        sim->insn[d].opt[3] = OPT_NONE;
      }

      sim->reloc[i] = d;
    } else {
      sim->insn[--d] = sim->insn[i];
      sim->reloc[i] = d;
    }
  sim->num_insn += num_call * 6;
  for (i = 0; i < sim->num_insn; i++)
    for (j = 0; j < MAX_OPERANDS; j++)
      if (sim->insn[i].opt[j] & OPT_REF || sim->insn[i].opt[j] & OPT_JUMP)
        sim->insn[i].op[j] = sim->reloc[sim->insn[i].op[j]];
}

/* Loads function from file into global array sim->insn.
   Function returns nonzero if function cannot be converted. */
int cuc_load (or1ksim *sim, char *in_fn)
{
  int i, j;
  FILE *fi;
  int func_return = 0;
  sim->num_insn = 0;

  log ("Loading filename %s\n", in_fn);
  if ((fi = fopen (in_fn, "rt")) == NULL) {
    fprintf (stderr, "Cannot open '%s'\n", in_fn);
    exit (1);
  }
  /* Read in the function and decode the instructions */
  for (i = 0;; i++) {
    unsigned long data;
    const char *name;

    if (fscanf (fi, "%08lx\n", &data) != 1) break;

    /* build params */
    name = build_insn (sim, data, &sim->insn[i]);
    if (func_return) func_return++;
    //PRINTF ("%s\n", name);

    if (or32_opcodes[sim->insn[i].index].flags & OR32_IF_DELAY) {
      int f;
      if (strcmp (name, "l.bnf") == 0) f = 1;
      else if (strcmp (name, "l.bf") == 0) f = 0;
      else if (strcmp (name, "l.j") == 0) {
	f = -1;
      } else if (strcmp (name, "l.jr") == 0 && func_return == 0) {
        func_return = 1;
        change_insn_type (&sim->insn[i], II_NOP);
        continue;
      } else {
        cucdebug (1, "Instruction #%i: \"%s\" not supported.\n", i, name);
        log ("Instruction #%i: \"%s\" not supported.\n", i, name);
        return 1;
      }
      if (f < 0) { /* l.j */
	/* repair params */
        change_insn_type (&sim->insn[i], II_BF);
        sim->insn[i].op[0] = i + sim->insn[i].op[0]; sim->insn[i].opt[0] = OPT_JUMP;
        sim->insn[i].op[1] = 1; sim->insn[i].opt[1] = OPT_CONST;
        sim->insn[i].type |= IT_BRANCH | IT_VOLATILE;
      } else {
        change_insn_type (&sim->insn[i], II_BF);
        sim->insn[i].op[0] = i + sim->insn[i].op[0]; sim->insn[i].opt[0] = OPT_JUMP;
        sim->insn[i].op[1] = FLAG_REG; sim->insn[i].opt[1] = OPT_REGISTER;
        sim->insn[i].type |= IT_BRANCH | IT_VOLATILE;
        if (f) sim->insn[i].type |= IT_FLAG1;
      }
    } else {
      sim->insn[i].index = -1;
      for (j = 0; j < sizeof (conv) / sizeof (cuc_conv); j++)
        if (strcmp (conv[j].from, name) == 0) {
	  if (conv[j].to & II_SIGNED) sim->insn[i].type |= IT_SIGNED;
	  if (conv[j].to & II_MEM) sim->insn[i].type |= IT_MEMORY | IT_VOLATILE;
          change_insn_type (&sim->insn[i], conv[j].to & II_MASK);
          break;
        }
      if (strcmp (name, "l.movhi") == 0) {
        sim->insn[i].op[1] <<= 16;
        sim->insn[i].op[2] = 0;
        sim->insn[i].opt[2] = OPT_CONST;
      }
      if (sim->insn[i].index == II_SFEQ || sim->insn[i].index == II_SFNE
       || sim->insn[i].index == II_SFLE || sim->insn[i].index == II_SFGT
       || sim->insn[i].index == II_SFGE || sim->insn[i].index == II_SFLT) {
	/* repair params */
        sim->insn[i].op[2] = sim->insn[i].op[1]; sim->insn[i].opt[2] = sim->insn[i].opt[1] & ~OPT_DEST;
        sim->insn[i].op[1] = sim->insn[i].op[0]; sim->insn[i].opt[1] = sim->insn[i].opt[0] & ~OPT_DEST;
        sim->insn[i].op[0] = FLAG_REG; sim->insn[i].opt[0] = OPT_DEST | OPT_REGISTER;
        sim->insn[i].opt[3] = OPT_NONE;
        sim->insn[i].type |= IT_COND;
      }
      if ((sim->insn[i].index < 0) ||
	  ((sim->insn[i].index == II_NOP) && (sim->insn[i].op[0] != 0))) {
        cucdebug (1, "Instruction #%i: \"%s\" not supported (2).\n", i, name);
        log ("Instruction #%i: \"%s\" not supported (2).\n", i, name);
        return 1;
      }
    }
  }
  sim->num_insn = i;
  fclose (fi);
  if (func_return != 2) {
    cucdebug (1, "Unsupported function structure.\n");
    log ("Unsupported function structure.\n");
    return 1;
  }

  log ("Number of instructions loaded = %i\n", sim->num_insn);
  if (sim->cuc_debug >= 3) print_cuc_insns (sim, "INITIAL", 1);

  log ("Converting.\n");
  expand_branch (sim);
  if (sim->cuc_debug >= 6) print_cuc_insns (sim, "AFTER_EXP_BRANCH", 0);

  remove_dslots (sim);
  if (sim->cuc_debug >= 6) print_cuc_insns (sim, "NO_DELAY_SLOTS", 0);

  if (sim->config.cuc.calling_convention) {
    detect_locals (sim);
    if (sim->cuc_debug >= 7) print_cuc_insns (sim, "AFTER_LOCALS", 0);
  }
  expand_memory (sim);
  if (sim->cuc_debug >= 3) print_cuc_insns (sim, "AFTER_EXP_MEM", 0);

  expand_signed (sim);
  if (sim->cuc_debug >= 3) print_cuc_insns (sim, "AFTER_EXP_SIG", 0);

  expand_calls (sim);
  if (sim->cuc_debug >= 3) print_cuc_insns (sim, "AFTER_EXP_CALLS", 0);

  return 0;
}
