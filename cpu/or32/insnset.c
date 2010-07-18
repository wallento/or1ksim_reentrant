/* insnset.c -- Instruction specific functions.

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
                 2000-2002 Marko Mlinar, markom@opencores.org
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


INSTRUCTION (l_add) {
  orreg_t temp1, temp2, temp3;
  int8_t temp4;
  
  temp2 = (orreg_t)PARAM2;
  temp3 = (orreg_t)PARAM1;
  temp1 = temp2 + temp3;
  SET_PARAM0(temp1);
  SET_OV_FLAG_FN (temp1);
  if (ARITH_SET_FLAG) {
    if(!temp1)
      sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
    else
      sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
  }
  if ((uorreg_t) temp1 < (uorreg_t) temp2)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_CY;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_CY;

  temp4 = temp1;
  if (temp4 == temp1)
    sim->or1k_mstats.byteadd++;
}
INSTRUCTION (l_addc) {
  orreg_t temp1, temp2, temp3;
  int8_t temp4;

  temp2 = (orreg_t)PARAM2;
  temp3 = (orreg_t)PARAM1;
  temp1 = temp2 + temp3;
  if(sim->cpu_state.sprs[SPR_SR] & SPR_SR_CY)
    temp1++;
  SET_PARAM0(temp1);
  SET_OV_FLAG_FN (temp1);
  if (ARITH_SET_FLAG) {
    if(!temp1)
      sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
    else
      sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
  }
  if ((uorreg_t) temp1 < (uorreg_t) temp2)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_CY;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_CY;

  temp4 = temp1;
  if (temp4 == temp1)
    sim->or1k_mstats.byteadd++;
}
INSTRUCTION (l_sw) {
  int old_cyc = 0;
  if (sim->config.cpu.sbuf_len) old_cyc = sim->runtime.sim.mem_cycles;
  set_mem32(sim,PARAM0, PARAM1, &sim->breakpoint);
  if (sim->config.cpu.sbuf_len) {
    int t = sim->runtime.sim.mem_cycles;
    sim->runtime.sim.mem_cycles = old_cyc;
    sbuf_store (sim,t - old_cyc);
  }
}
INSTRUCTION (l_sb) {
  int old_cyc = 0;
  if (sim->config.cpu.sbuf_len) old_cyc = sim->runtime.sim.mem_cycles;
  set_mem8(sim,PARAM0, PARAM1, &sim->breakpoint);
  if (sim->config.cpu.sbuf_len) {
    int t = sim->runtime.sim.mem_cycles;
    sim->runtime.sim.mem_cycles = old_cyc;
    sbuf_store (sim,t- old_cyc);
  }
}
INSTRUCTION (l_sh) {
  int old_cyc = 0;
  if (sim->config.cpu.sbuf_len) old_cyc = sim->runtime.sim.mem_cycles;
  set_mem16(sim,PARAM0, PARAM1, &sim->breakpoint);
  if (sim->config.cpu.sbuf_len) {
    int t = sim->runtime.sim.mem_cycles;
    sim->runtime.sim.mem_cycles = old_cyc;
    sbuf_store (sim,t - old_cyc);
  }
}
INSTRUCTION (l_lwz) {
  uint32_t val;
  if (sim->config.cpu.sbuf_len) sbuf_load(sim);
  val = eval_mem32(sim,PARAM1, &sim->breakpoint);
  /* If eval operand produced exception don't set anything. JPB changed to
     trigger on breakpoint, as well as sim->except_pending (seemed to be a bug). */
  if (!(sim->except_pending || sim->breakpoint))
    SET_PARAM0(val);
}
INSTRUCTION (l_lbs) {
  int8_t val;
  if (sim->config.cpu.sbuf_len) sbuf_load(sim);
  val = eval_mem8(sim,PARAM1, &sim->breakpoint);
  /* If eval operand produced exception don't set anything. JPB changed to
     trigger on breakpoint, as well as sim->except_pending (seemed to be a bug). */
  if (!(sim->except_pending || sim->breakpoint))
    SET_PARAM0(val);
}
INSTRUCTION (l_lbz) {
  uint8_t val;
  if (sim->config.cpu.sbuf_len) sbuf_load(sim);
  val = eval_mem8(sim,PARAM1, &sim->breakpoint);
  /* If eval operand produced exception don't set anything. JPB changed to
     trigger on breakpoint, as well as sim->except_pending (seemed to be a bug). */
  if (!(sim->except_pending || sim->breakpoint))
    SET_PARAM0(val);
}
INSTRUCTION (l_lhs) {
  int16_t val;
  if (sim->config.cpu.sbuf_len) sbuf_load(sim);
  val = eval_mem16(sim,PARAM1, &sim->breakpoint);
  /* If eval operand produced exception don't set anything. JPB changed to
     trigger on breakpoint, as well as sim->except_pending (seemed to be a bug). */
  if (!(sim->except_pending || sim->breakpoint))
    SET_PARAM0(val);
}
INSTRUCTION (l_lhz) {
  uint16_t val;
  if (sim->config.cpu.sbuf_len) sbuf_load(sim);
  val = eval_mem16(sim,PARAM1, &sim->breakpoint);
  /* If eval operand produced exception don't set anything. JPB changed to
     trigger on breakpoint, as well as sim->except_pending (seemed to be a bug). */
  if (!(sim->except_pending || sim->breakpoint))
    SET_PARAM0(val);
}
INSTRUCTION (l_movhi) {
  SET_PARAM0(PARAM1 << 16);
}
INSTRUCTION (l_and) {
  uorreg_t temp1;
  temp1 = PARAM1 & PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
  if (ARITH_SET_FLAG) {
    if(!temp1)
      sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
    else
      sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
  }
}
INSTRUCTION (l_or) {
  uorreg_t temp1;
  temp1 = PARAM1 | PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
}
INSTRUCTION (l_xor) {
  uorreg_t temp1;
  temp1 = PARAM1 ^ PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
}
INSTRUCTION (l_sub) {
  orreg_t temp1;
  temp1 = (orreg_t)PARAM1 - (orreg_t)PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
}
/*int mcount = 0;*/
INSTRUCTION (l_mul) {
  orreg_t temp1;

  temp1 = (orreg_t)PARAM1 * (orreg_t)PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
  /*if (!(mcount++ & 1023)) {
    PRINTF ("[%i]\n",mcount);
    }*/
}
INSTRUCTION (l_div) {
  orreg_t temp3, temp2, temp1;

  temp3 = PARAM2;
  temp2 = PARAM1;
  if (temp3)
    temp1 = temp2 / temp3;
  else {
    except_handle(sim,EXCEPT_ILLEGAL, sim->cpu_state.pc);
    return;
  }
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
}
INSTRUCTION (l_divu) {
  uorreg_t temp3, temp2, temp1;

  temp3 = PARAM2;
  temp2 = PARAM1;
  if (temp3)
    temp1 = temp2 / temp3;
  else {
    except_handle(sim,EXCEPT_ILLEGAL, sim->cpu_state.pc);
    return;
  }
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
  /* sim->runtime.sim.cycles += 16; */
}
INSTRUCTION (l_sll) {
  uorreg_t temp1;

  temp1 = PARAM1 << PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
  /* sim->runtime.sim.cycles += 2; */
}
INSTRUCTION (l_sra) {
  orreg_t temp1;

  temp1 = (orreg_t)PARAM1 >> PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
  /* sim->runtime.sim.cycles += 2; */
}
INSTRUCTION (l_srl) {
  uorreg_t temp1;
  temp1 = PARAM1 >> PARAM2;
  SET_OV_FLAG_FN (temp1);
  SET_PARAM0(temp1);
  /* sim->runtime.sim.cycles += 2; */
}
INSTRUCTION (l_bf) {
  if (sim->config.bpb.enabled) {
    int fwd = (PARAM0 >= sim->cpu_state.pc) ? 1 : 0;
    sim->or1k_mstats.bf[sim->cpu_state.sprs[SPR_SR] & SPR_SR_F ? 1 : 0][fwd]++;
    bpb_update(sim, current->insn_addr, sim->cpu_state.sprs[SPR_SR] & SPR_SR_F ? 1 : 0);
  }
  if(sim->cpu_state.sprs[SPR_SR] & SPR_SR_F) {
    sim->cpu_state.pc_delay = sim->cpu_state.pc + (orreg_t)PARAM0 * 4;
    btic_update(sim, sim->pcnext);
    sim->next_delay_insn = 1;
  } else {
    btic_update(sim, sim->cpu_state.pc);
  }
}
INSTRUCTION (l_bnf) {
  if (sim->config.bpb.enabled) {
    int fwd = (PARAM0 >= sim->cpu_state.pc) ? 1 : 0;
    sim->or1k_mstats.bnf[sim->cpu_state.sprs[SPR_SR] & SPR_SR_F ? 0 : 1][fwd]++;
    bpb_update(sim, current->insn_addr, sim->cpu_state.sprs[SPR_SR] & SPR_SR_F ? 0 : 1);
  }
  if (!(sim->cpu_state.sprs[SPR_SR] & SPR_SR_F)) {
    sim->cpu_state.pc_delay = sim->cpu_state.pc + (orreg_t)PARAM0 * 4;
    btic_update(sim, sim->pcnext);
    sim->next_delay_insn = 1;
  } else {
    btic_update(sim, sim->cpu_state.pc);
  }
}
INSTRUCTION (l_j) {
  sim->cpu_state.pc_delay = sim->cpu_state.pc + (orreg_t)PARAM0 * 4;
  sim->next_delay_insn = 1;
}
INSTRUCTION (l_jal) {
  sim->cpu_state.pc_delay = sim->cpu_state.pc + (orreg_t)PARAM0 * 4;

  setsim_reg(sim, LINK_REGNO, sim->cpu_state.pc + 8);
  sim->next_delay_insn = 1;
  if (sim->config.sim.profile) {
    struct label_entry *tmp;
    if (verify_memoryarea(sim,sim->cpu_state.pc_delay) && (tmp = get_label (sim, sim->cpu_state.pc_delay)))
      fprintf (sim->runtime.sim.fprof, "+%08llX %"PRIxADDR" %"PRIxADDR" %s\n",
               sim->runtime.sim.cycles, sim->cpu_state.pc + 8, sim->cpu_state.pc_delay,
               tmp->name);
    else
      fprintf (sim->runtime.sim.fprof, "+%08llX %"PRIxADDR" %"PRIxADDR" @%"PRIxADDR"\n",
               sim->runtime.sim.cycles, sim->cpu_state.pc + 8, sim->cpu_state.pc_delay,
               sim->cpu_state.pc_delay);
  }
}
INSTRUCTION (l_jalr) {
  sim->cpu_state.pc_delay = PARAM0;
  setsim_reg(sim, LINK_REGNO, sim->cpu_state.pc + 8);
  sim->next_delay_insn = 1;
}
INSTRUCTION (l_jr) {
  sim->cpu_state.pc_delay = PARAM0;
  sim->next_delay_insn = 1;
  if (sim->config.sim.profile)
    fprintf (sim->runtime.sim.fprof, "-%08llX %"PRIxADDR"\n", sim->runtime.sim.cycles,
             sim->cpu_state.pc_delay);
}
INSTRUCTION (l_rfe) {
  sim->pcnext = sim->cpu_state.sprs[SPR_EPCR_BASE];
  mtspr(sim,SPR_SR, sim->cpu_state.sprs[SPR_ESR_BASE]);
}
INSTRUCTION (l_nop) {
  oraddr_t stackaddr;
  uint32_t k = PARAM0;
  switch (k) {
    case NOP_NOP:
      break;
    case NOP_EXIT:
      PRINTF("exit(%"PRIdREG")\n", evalsim_reg (sim,3));
      fprintf(stderr, "@reset : cycles %lld, insn #%lld\n",
              sim->runtime.sim.reset_cycles, sim->runtime.cpu.reset_instructions);
      fprintf(stderr, "@exit  : cycles %lld, insn #%lld\n", sim->runtime.sim.cycles,
              sim->runtime.cpu.instructions);
      fprintf(stderr, " diff  : cycles %lld, insn #%lld\n",
              sim->runtime.sim.cycles - sim->runtime.sim.reset_cycles,
              sim->runtime.cpu.instructions - sim->runtime.cpu.reset_instructions);
      if (sim->config.debug.gdb_enabled)
        set_stall_state (sim, 1);
      else
        sim_done(sim);
      break;
    case NOP_CNT_RESET:
      PRINTF("****************** counters reset ******************\n");
      PRINTF("cycles %lld, insn #%lld\n", sim->runtime.sim.cycles, sim->runtime.cpu.instructions);
      PRINTF("****************** counters reset ******************\n");
      sim->runtime.sim.reset_cycles = sim->runtime.sim.cycles;
      sim->runtime.cpu.reset_instructions = sim->runtime.cpu.instructions;
      break;
    case NOP_PRINTF:
      stackaddr = evalsim_reg(sim, 4);
      simprintf(stackaddr, evalsim_reg(sim, 3));
      break;
    case NOP_PUTC:		/*JPB */
      printf( "%c", (char)(evalsim_reg(sim, 3 ) & 0xff));
      fflush( stdout );
      break;
    case NOP_REPORT:
      PRINTF("report(0x%"PRIxREG");\n", evalsim_reg(sim, 3));
    default:
      if (k >= NOP_REPORT_FIRST && k <= NOP_REPORT_LAST)
      PRINTF("report %" PRIdREG " (0x%"PRIxREG");\n", k - NOP_REPORT_FIRST,
             evalsim_reg(sim, 3));
      break;
  }
}
INSTRUCTION (l_sfeq) {
  if(PARAM0 == PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfne) {
  if(PARAM0 != PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfgts) {
  if((orreg_t)PARAM0 > (orreg_t)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfges) {
  if((orreg_t)PARAM0 >= (orreg_t)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sflts) {
  if((orreg_t)PARAM0 < (orreg_t)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfles) {
  if((orreg_t)PARAM0 <= (orreg_t)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfgtu) {
  if(PARAM0 > PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfgeu) {
  if(PARAM0 >= PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfltu) {
  if(PARAM0 < PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_sfleu) {
  if(PARAM0 <= PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (l_extbs) {
  int8_t x;
  x = PARAM1;
  SET_PARAM0((orreg_t)x);
}
INSTRUCTION (l_extbz) {
  uint8_t x;
  x = PARAM1;
  SET_PARAM0((uorreg_t)x);
}
INSTRUCTION (l_exths) {
  int16_t x;
  x = PARAM1;
  SET_PARAM0((orreg_t)x);
}
INSTRUCTION (l_exthz) {
  uint16_t x;
  x = PARAM1;
  SET_PARAM0((uorreg_t)x);
}
INSTRUCTION (l_extws) {
  int32_t x;
  x = PARAM1;
  SET_PARAM0((orreg_t)x);
}
INSTRUCTION (l_extwz) {
  uint32_t x;
  x = PARAM1;
  SET_PARAM0((uorreg_t)x);
}
INSTRUCTION (l_mtspr) {
  uint16_t regno = PARAM0 + PARAM2;
  uorreg_t value = PARAM1;

  if (sim->cpu_state.sprs[SPR_SR] & SPR_SR_SM)
    mtspr(sim,regno, value);
  else {
    PRINTF("WARNING: trying to write SPR while SR[SUPV] is cleared.\n");
    sim_done(sim);
  }
}
INSTRUCTION (l_mfspr) {
  uint16_t regno = PARAM1 + PARAM2;
  uorreg_t value = mfspr(sim,regno);

  if (sim->cpu_state.sprs[SPR_SR] & SPR_SR_SM)
    SET_PARAM0(value);
  else {
    SET_PARAM0(0);
    PRINTF("WARNING: trying to read SPR while SR[SUPV] is cleared.\n");
    sim_done(sim);
  }
}
INSTRUCTION (l_sys) {
  except_handle(sim,EXCEPT_SYSCALL, sim->cpu_state.sprs[SPR_EEAR_BASE]);
}
INSTRUCTION (l_trap) {
  /* TODO: some SR related code here! */
  except_handle(sim,EXCEPT_TRAP, sim->cpu_state.sprs[SPR_EEAR_BASE]);
}
INSTRUCTION (l_mac) {
  uorreg_t lo, hi;
  LONGEST l;
  orreg_t x, y;

  lo = sim->cpu_state.sprs[SPR_MACLO];
  hi = sim->cpu_state.sprs[SPR_MACHI];
  x = PARAM0;
  y = PARAM1;
/*   PRINTF ("[%"PRIxREG",%"PRIxREG"]\t", x, y); */
  l = (ULONGEST)lo | ((LONGEST)hi << 32);
  l += (LONGEST) x * (LONGEST) y;

  /* This implementation is very fast - it needs only one cycle for mac.  */
  lo = ((ULONGEST)l) & 0xFFFFFFFF;
  hi = ((LONGEST)l) >> 32;
  sim->cpu_state.sprs[SPR_MACLO] = lo;
  sim->cpu_state.sprs[SPR_MACHI] = hi;
/*   PRINTF ("(%"PRIxREG",%"PRIxREG"\n", hi, lo); */
}
INSTRUCTION (l_msb) {
  uorreg_t lo, hi;
  LONGEST l;
  orreg_t x, y;

  lo = sim->cpu_state.sprs[SPR_MACLO];
  hi = sim->cpu_state.sprs[SPR_MACHI];
  x = PARAM0;
  y = PARAM1;

/*   PRINTF ("[%"PRIxREG",%"PRIxREG"]\t", x, y); */

  l = (ULONGEST)lo | ((LONGEST)hi << 32);
  l -= x * y;

  /* This implementation is very fast - it needs only one cycle for msb.  */
  lo = ((ULONGEST)l) & 0xFFFFFFFF;
  hi = ((LONGEST)l) >> 32;
  sim->cpu_state.sprs[SPR_MACLO] = lo;
  sim->cpu_state.sprs[SPR_MACHI] = hi;
/*   PRINTF ("(%"PRIxREG",%"PRIxREG")\n", hi, lo); */
}
INSTRUCTION (l_macrc) {
  uorreg_t lo, hi;
  LONGEST l;
  /* No need for synchronization here -- all MAC instructions are 1 cycle long.  */
  lo =  sim->cpu_state.sprs[SPR_MACLO];
  hi =  sim->cpu_state.sprs[SPR_MACHI];
  l = (ULONGEST) lo | ((LONGEST)hi << 32);
  l >>= 28;
  //PRINTF ("<%08x>\n", (unsigned long)l);
  SET_PARAM0((orreg_t)l);
  sim->cpu_state.sprs[SPR_MACLO] = 0;
  sim->cpu_state.sprs[SPR_MACHI] = 0;
}
INSTRUCTION (l_cmov) {
  SET_PARAM0(sim->cpu_state.sprs[SPR_SR] & SPR_SR_F ? PARAM1 : PARAM2);
}
INSTRUCTION (l_ff1) {
  SET_PARAM0(ffs(PARAM1));
}
/******* Floating point instructions *******/
/* Single precision */
INSTRUCTION (lf_add_s) {
  SET_PARAM0((float)PARAM1 + (float)PARAM2);
}
INSTRUCTION (lf_div_s) {
  SET_PARAM0((float)PARAM1 / (float)PARAM2);
}
INSTRUCTION (lf_ftoi_s) {
//  set_operand32(0, freg[get_operand(1)], &sim->breakpoint);
}
INSTRUCTION (lf_itof_s) {
//  freg[get_operand(0)] = eval_operand32(1, &sim->breakpoint);
}
INSTRUCTION (lf_madd_s) {
  SET_PARAM0((float)PARAM0 + (float)PARAM1 * (float)PARAM2);
}
INSTRUCTION (lf_mul_s) {
  SET_PARAM0((float)PARAM1 * (float)PARAM2);
}
INSTRUCTION (lf_rem_s) {
  float temp = (float)PARAM1 / (float)PARAM2;
  SET_PARAM0(temp - (uint32_t)temp);
}
INSTRUCTION (lf_sfeq_s) {
  if((float)PARAM0 == (float)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (lf_sfge_s) {
  if((float)PARAM0 >= (float)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (lf_sfgt_s) {
  if((float)PARAM0 > (float)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (lf_sfle_s) {
  if((float)PARAM0 <= (float)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (lf_sflt_s) {
  if((float)PARAM0 < (float)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (lf_sfne_s) {
  if((float)PARAM0 != (float)PARAM1)
    sim->cpu_state.sprs[SPR_SR] |= SPR_SR_F;
  else
    sim->cpu_state.sprs[SPR_SR] &= ~SPR_SR_F;
}
INSTRUCTION (lf_sub_s) {
  SET_PARAM0((float)PARAM1 - (float)PARAM2);
}

/******* Custom instructions *******/
INSTRUCTION (l_cust1) {
  /*int destr = current->insn >> 21;
    int src1r = current->insn >> 15;
    int src2r = current->insn >> 9;*/
}
INSTRUCTION (l_cust2) {
}
INSTRUCTION (l_cust3) {
}
INSTRUCTION (l_cust4) {
}
