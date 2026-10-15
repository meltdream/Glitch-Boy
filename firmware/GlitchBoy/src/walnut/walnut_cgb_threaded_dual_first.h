/* Generated mechanically from walnut_cgb.h dualfetch first opcode switch. */
first_00: /* NOP */
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_01: /* LD BC, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
    WGB_CPU_REG.bc.bytes.c = (uint8_t)(oppair >> 8); // C was already partially loaded in oppair
    oppair = __gb_read16(gb, WGB_CPU_REG.pc.reg + 1); // Read 16-bit immediate starting from PC+1
    WGB_CPU_REG.bc.bytes.b = (uint8_t)(oppair);// Store lower byte of immediate into B
    WGB_CPU_REG.pc.reg += 2;// Increment PC by 2 to skip over the 16-bit immediate
    // Prefetch next opcode from upper byte of read16
    opcode = (uint8_t)(oppair >> 8);
#else
    // Original 8-bit fetch path
    WGB_CPU_REG.bc.bytes.c = (uint8_t)(oppair >> 8);
    WGB_CPU_REG.pc.reg++;
    WGB_CPU_REG.bc.bytes.b = __gb_fetch8(gb);
    opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#endif
		goto threaded_dual_first_done;
first_02: /* LD (BC), A */
		__gb_write(gb, WGB_CPU_REG.bc.reg, WGB_CPU_REG.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_03: /* INC BC */
		WGB_CPU_REG.bc.reg++;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_04: /* INC B */
		WGB_INSTR_INC_R8(WGB_CPU_REG.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_05: /* DEC B */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_06: /* LD B, imm */
		WGB_CPU_REG.bc.bytes.b = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
first_07: /* RLCA */
		WGB_CPU_REG.a = (WGB_CPU_REG.a << 1) | (WGB_CPU_REG.a >> 7);
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = (WGB_CPU_REG.a & 0x01);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
first_08: /* LD (imm), SP */
	{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
		uint8_t l = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
    oppair = __gb_read16(gb, WGB_CPU_REG.pc.reg++);
		uint8_t h = oppair;
		uint16_t temp = WALNUT_GB_U8_TO_U16(h, l);
		__gb_write16(gb,temp,WGB_CPU_REG.sp.reg);
    // WGB_CPU_REG.pc.reg+=2;
		opcode = oppair >> 8;
#else
    uint8_t l = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
    uint8_t h = __gb_fetch8(gb);
    uint16_t temp = WALNUT_GB_U8_TO_U16(h, l);
    __gb_write(gb, temp++, WGB_CPU_REG.sp.bytes.p);
    __gb_write(gb, temp, WGB_CPU_REG.sp.bytes.s);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#endif
    goto threaded_dual_first_done;
	}

first_09: /* ADD HL, BC */
	{
		uint_fast32_t temp = WGB_CPU_REG.hl.reg + WGB_CPU_REG.bc.reg;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h =
			(temp ^ WGB_CPU_REG.hl.reg ^ WGB_CPU_REG.bc.reg) & 0x1000 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		WGB_CPU_REG.hl.reg = (temp & 0x0000FFFF);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_0A: /* LD A, (BC) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.bc.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_0B: /* DEC BC */
		WGB_CPU_REG.bc.reg--;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_0C: /* INC C */
		WGB_INSTR_INC_R8(WGB_CPU_REG.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_0D: /* DEC C */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_0E: /* LD C, imm */
		WGB_CPU_REG.bc.bytes.c = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
	  opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_0F: /* RRCA */
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = WGB_CPU_REG.a & 0x01;
		WGB_CPU_REG.a = (WGB_CPU_REG.a >> 1) | (WGB_CPU_REG.a << 7);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
first_10: /* STOP */
		//gb->gb_halt = true;
#if WALNUT_FULL_GBC_SUPPORT
		if(gb->cgb.cgbMode & gb->cgb.doubleSpeedPrep)
		{
			/* Batched cycles ran at the old speed; apply them first. */
			__gb_lazy_post_flush_gb(gb);
			gb->cgb.doubleSpeedPrep = 0;
			gb->cgb.doubleSpeed ^= 1;
			__gb_cpu_timing_reschedule(gb);
		}
#endif
		/* STOP is two bytes; the second is skipped, not executed (some games
		 * pad it with a non-NOP byte, e.g. 10 06). */
		WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_11: /* LD DE, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
    WGB_CPU_REG.de.bytes.e = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
		oppair = __gb_read16(gb, WGB_CPU_REG.pc.reg++);
    WGB_CPU_REG.de.bytes.d = oppair;
		opcode = oppair >> 8;
#else
    WGB_CPU_REG.de.bytes.e = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
    WGB_CPU_REG.de.bytes.d = __gb_fetch8(gb);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#endif
		goto threaded_dual_first_done;
first_12: /* LD (DE), A */
		__gb_write(gb, WGB_CPU_REG.de.reg, WGB_CPU_REG.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_13: /* INC DE */
		WGB_CPU_REG.de.reg++;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_14: /* INC D */
		WGB_INSTR_INC_R8(WGB_CPU_REG.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_15: /* DEC D */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_16: /* LD D, imm */
		WGB_CPU_REG.de.bytes.d = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
	  opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_17: /* RLA */
	{
		uint8_t temp = WGB_CPU_REG.a;
		WGB_CPU_REG.a = (WGB_CPU_REG.a << 1) | WGB_CPU_REG.f.f_bits.c;
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = (temp >> 7) & 0x01;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_18: /* JR imm */
	{
		int8_t temp = (int8_t) (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
		WGB_CPU_REG.pc.reg += temp;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}
first_19: /* ADD HL, DE */
	{
		uint_fast32_t temp = WGB_CPU_REG.hl.reg + WGB_CPU_REG.de.reg;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h =
			(temp ^ WGB_CPU_REG.hl.reg ^ WGB_CPU_REG.de.reg) & 0x1000 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		WGB_CPU_REG.hl.reg = (temp & 0x0000FFFF);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}
first_1A: /* LD A, (DE) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.de.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_1B: /* DEC DE */
		WGB_CPU_REG.de.reg--;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_1C: /* INC E */
		WGB_INSTR_INC_R8(WGB_CPU_REG.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_1D: /* DEC E */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_1E: /* LD E, imm */
		WGB_CPU_REG.de.bytes.e =  (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
	  opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_1F: /* RRA */
	{
		uint8_t temp = WGB_CPU_REG.a;
		WGB_CPU_REG.a = WGB_CPU_REG.a >> 1 | (WGB_CPU_REG.f.f_bits.c << 7);
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = temp & 0x1;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_20: /* JR NZ, imm */
		if(!WGB_CPU_REG.f.f_bits.z)
		{
			int8_t temp = (int8_t) (oppair >> 8);
	    WGB_CPU_REG.pc.reg++;
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg, hotpc_fetch_pc, 0x20);
		}
		else
			WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_21: /* LD HL, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
		WGB_CPU_REG.hl.bytes.l =(uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		oppair = __gb_read16(gb, WGB_CPU_REG.pc.reg++);
    WGB_CPU_REG.hl.bytes.h = oppair;
		opcode = oppair >> 8;
#else
    WGB_CPU_REG.hl.bytes.l =(uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
    WGB_CPU_REG.hl.bytes.h = __gb_fetch8(gb);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#endif
		goto threaded_dual_first_done;

first_22: /* LDI (HL), A */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.a);
		WGB_CPU_REG.hl.reg++;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_23: /* INC HL */
		WGB_CPU_REG.hl.reg++;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_24: /* INC H */
		WGB_INSTR_INC_R8(WGB_CPU_REG.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_25: /* DEC H */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_26: /* LD H, imm */
		WGB_CPU_REG.hl.bytes.h =(uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
	  opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_27: /* DAA */
	{
		/* The following is from SameBoy. MIT License. */
		int16_t a = WGB_CPU_REG.a;

		if(WGB_CPU_REG.f.f_bits.n)
		{
			if(WGB_CPU_REG.f.f_bits.h)
				a = (a - 0x06) & 0xFF;

			if(WGB_CPU_REG.f.f_bits.c)
				a -= 0x60;
		}
		else
		{
			if(WGB_CPU_REG.f.f_bits.h || (a & 0x0F) > 9)
				a += 0x06;

			if(WGB_CPU_REG.f.f_bits.c || a > 0x9F)
				a += 0x60;
		}

		if((a & 0x100) == 0x100)
			WGB_CPU_REG.f.f_bits.c = 1;

		WGB_CPU_REG.a = (uint8_t)a;
		WGB_CPU_REG.f.f_bits.z = (WGB_CPU_REG.a == 0);
		WGB_CPU_REG.f.f_bits.h = 0;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_28: /* JR Z, imm */
		if(WGB_CPU_REG.f.f_bits.z)
		{
			int8_t temp = (int8_t) (oppair >> 8);
	    WGB_CPU_REG.pc.reg++;
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg, hotpc_fetch_pc, 0x28);
		}
		else
			WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_29: /* ADD HL, HL */
	{
		WGB_CPU_REG.f.f_bits.c = (WGB_CPU_REG.hl.reg & 0x8000) > 0;
		WGB_CPU_REG.hl.reg <<= 1;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h = (WGB_CPU_REG.hl.reg & 0x1000) > 0;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_2A: /* LD A, (HL+) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.hl.reg++);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_2B: /* DEC HL */
		WGB_CPU_REG.hl.reg--;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_2C: /* INC L */
		WGB_INSTR_INC_R8(WGB_CPU_REG.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_2D: /* DEC L */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_2E: /* LD L, imm */
		WGB_CPU_REG.hl.bytes.l = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
	  opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_2F: /* CPL */
		WGB_CPU_REG.a = ~WGB_CPU_REG.a;
		WGB_CPU_REG.f.f_bits.n = 1;
		WGB_CPU_REG.f.f_bits.h = 1;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_30: /* JR NC, imm */
		if(!WGB_CPU_REG.f.f_bits.c)
		{
			int8_t temp = (int8_t) (oppair >> 8);
	    WGB_CPU_REG.pc.reg++;
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					WGB_CPU_REG.pc.reg, hotpc_fetch_pc))
				__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg,
					hotpc_fetch_pc, 0x30);
#endif
		}
		else
			WGB_CPU_REG.pc.reg++;
    opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_31: /* LD SP, imm */
#if WALNUT_GB_16_BIT_OPS_DUALFETCH
		WGB_CPU_REG.sp.bytes.p = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg ++;
    oppair = __gb_read16(gb, WGB_CPU_REG.pc.reg++);
		WGB_CPU_REG.sp.bytes.s = oppair; // auto-truncated
		opcode = oppair >> 8;
#else
    WGB_CPU_REG.sp.bytes.p = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
    WGB_CPU_REG.sp.bytes.s = __gb_fetch8(gb);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#endif
		goto threaded_dual_first_done;

first_32: /* LD (HL), A */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.a);
		WGB_CPU_REG.hl.reg--;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_33: /* INC SP */
		WGB_CPU_REG.sp.reg++;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_34: /* INC (HL) */
	{
		uint8_t temp = __gb_read(gb, WGB_CPU_REG.hl.reg);
		WGB_INSTR_INC_R8(temp);
		__gb_write(gb, WGB_CPU_REG.hl.reg, temp);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_35: /* DEC (HL) */
	{
		uint8_t temp = __gb_read(gb, WGB_CPU_REG.hl.reg);
		WGB_INSTR_DEC_R8(temp);
		__gb_write(gb, WGB_CPU_REG.hl.reg, temp);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_36: /* LD (HL), imm */
		__gb_write(gb, WGB_CPU_REG.hl.reg,(uint8_t)(oppair >> 8));
		WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_37: /* SCF */
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h = 0;
		WGB_CPU_REG.f.f_bits.c = 1;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_38: /* JR C, imm */
		if(WGB_CPU_REG.f.f_bits.c)
		{
			int8_t temp = (int8_t)(oppair >> 8);
	    WGB_CPU_REG.pc.reg++;
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					WGB_CPU_REG.pc.reg, hotpc_fetch_pc))
				__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg,
					hotpc_fetch_pc, 0x38);
#endif
		}
		else
			WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_39: /* ADD HL, SP */
	{
		uint_fast32_t temp = WGB_CPU_REG.hl.reg + WGB_CPU_REG.sp.reg;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h =
			((WGB_CPU_REG.hl.reg & 0xFFF) + (WGB_CPU_REG.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = temp & 0x10000 ? 1 : 0;
		WGB_CPU_REG.hl.reg = (uint16_t)temp;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_3A: /* LD A, (HL) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.hl.reg--);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_3B: /* DEC SP */
		WGB_CPU_REG.sp.reg--;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_3C: /* INC A */
		WGB_INSTR_INC_R8(WGB_CPU_REG.a);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_3D: /* DEC A */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.a);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_3E: /* LD A, imm */
		WGB_CPU_REG.a = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
	  opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_3F: /* CCF */
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h = 0;
		WGB_CPU_REG.f.f_bits.c = ~WGB_CPU_REG.f.f_bits.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_40: /* LD B, B */
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_41: /* LD B, C */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_42: /* LD B, D */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_43: /* LD B, E */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_44: /* LD B, H */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_45: /* LD B, L */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_46: /* LD B, (HL) */
		WGB_CPU_REG.bc.bytes.b = __gb_read(gb, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_47: /* LD B, A */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.a;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_48: /* LD C, B */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_49: /* LD C, C */
	  opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_4A: /* LD C, D */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_4B: /* LD C, E */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_4C: /* LD C, H */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_4D: /* LD C, L */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_4E: /* LD C, (HL) */
		WGB_CPU_REG.bc.bytes.c = __gb_read(gb, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_4F: /* LD C, A */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.a;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_50: /* LD D, B */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_51: /* LD D, C */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_52: /* LD D, D */
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_53: /* LD D, E */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_54: /* LD D, H */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_55: /* LD D, L */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_56: /* LD D, (HL) */
		WGB_CPU_REG.de.bytes.d = __gb_read(gb, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_57: /* LD D, A */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.a;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_58: /* LD E, B */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_59: /* LD E, C */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_5A: /* LD E, D */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_5B: /* LD E, E */
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_5C: /* LD E, H */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_5D: /* LD E, L */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_5E: /* LD E, (HL) */
		WGB_CPU_REG.de.bytes.e = __gb_read(gb, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_5F: /* LD E, A */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.a;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_60: /* LD H, B */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_61: /* LD H, C */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_62: /* LD H, D */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_63: /* LD H, E */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_64: /* LD H, H */
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_65: /* LD H, L */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_66: /* LD H, (HL) */
		WGB_CPU_REG.hl.bytes.h = __gb_read(gb, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_67: /* LD H, A */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.a;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_68: /* LD L, B */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_69: /* LD L, C */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_6A: /* LD L, D */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_6B: /* LD L, E */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_6C: /* LD L, H */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_6D: /* LD L, L */
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_6E: /* LD L, (HL) */
		WGB_CPU_REG.hl.bytes.l = __gb_read(gb, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_6F: /* LD L, A */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.a;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_70: /* LD (HL), B */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.bc.bytes.b);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_71: /* LD (HL), C */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.bc.bytes.c);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_72: /* LD (HL), D */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.de.bytes.d);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_73: /* LD (HL), E */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_74: /* LD (HL), H */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.hl.bytes.h);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_75: /* LD (HL), L */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.hl.bytes.l);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_76: /* HALT */
	{
		/* TODO: Emulate HALT bug? */
		gb->gb_halt = true;

		if(__gb_halt_fast_forward_to_vblank(gb))
		{
			inst_cycles = 0;
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
			opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#else
			opcode = (uint8_t)(oppair >> 8);
#endif
			goto threaded_dual_first_done;
		}

#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // possibly changes mbc behaviour
#else
    opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;
	}


first_77: /* LD (HL), A */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // GTA SAFE TEST
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_78: /* LD A, B */
		WGB_CPU_REG.a = WGB_CPU_REG.bc.bytes.b;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_79: /* LD A, C */
		WGB_CPU_REG.a = WGB_CPU_REG.bc.bytes.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_7A: /* LD A, D */
		WGB_CPU_REG.a = WGB_CPU_REG.de.bytes.d;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_7B: /* LD A, E */
		WGB_CPU_REG.a = WGB_CPU_REG.de.bytes.e;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_7C: /* LD A, H */
		WGB_CPU_REG.a = WGB_CPU_REG.hl.bytes.h;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_7D: /* LD A, L */
		WGB_CPU_REG.a = WGB_CPU_REG.hl.bytes.l;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_7E: /* LD A, (HL) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_7F: /* LD A, A */
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_80: /* ADD A, B */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.b, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_81: /* ADD A, C */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.c, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_82: /* ADD A, D */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.d, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_83: /* ADD A, E */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.e, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_84: /* ADD A, H */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.h, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_85: /* ADD A, L */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.l, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_86: /* ADD A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_87: /* ADD A, A */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.a, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_88: /* ADC A, B */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.b, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_89: /* ADC A, C */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.c, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_8A: /* ADC A, D */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.d, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_8B: /* ADC A, E */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.e, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_8C: /* ADC A, H */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.h, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_8D: /* ADC A, L */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.l, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_8E: /* ADC A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_8F: /* ADC A, A */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.a, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_90: /* SUB B */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.b, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_91: /* SUB C */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.c, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_92: /* SUB D */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.d, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_93: /* SUB E */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.e, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_94: /* SUB H */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.h, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_95: /* SUB L */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.l, 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_96: /* SUB (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), 0);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_97: /* SUB A */
		WGB_CPU_REG.a = 0;
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.z = 1;
		WGB_CPU_REG.f.f_bits.n = 1;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_98: /* SBC A, B */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.b, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_99: /* SBC A, C */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.c, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_9A: /* SBC A, D */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.d, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_9B: /* SBC A, E */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.e, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_9C: /* SBC A, H */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.h, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_9D: /* SBC A, L */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.l, WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_9E: /* SBC A, (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), WGB_CPU_REG.f.f_bits.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_9F: /* SBC A, A */
		WGB_CPU_REG.a = WGB_CPU_REG.f.f_bits.c ? 0xFF : 0x00;
		WGB_CPU_REG.f.f_bits.z = !WGB_CPU_REG.f.f_bits.c;
		WGB_CPU_REG.f.f_bits.n = 1;
		WGB_CPU_REG.f.f_bits.h = WGB_CPU_REG.f.f_bits.c;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A0: /* AND B */
		WGB_INSTR_AND_R8(WGB_CPU_REG.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A1: /* AND C */
		WGB_INSTR_AND_R8(WGB_CPU_REG.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A2: /* AND D */
		WGB_INSTR_AND_R8(WGB_CPU_REG.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A3: /* AND E */
		WGB_INSTR_AND_R8(WGB_CPU_REG.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A4: /* AND H */
		WGB_INSTR_AND_R8(WGB_CPU_REG.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A5: /* AND L */
		WGB_INSTR_AND_R8(WGB_CPU_REG.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A6: /* AND (HL) */
		WGB_INSTR_AND_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A7: /* AND A */
		WGB_INSTR_AND_R8(WGB_CPU_REG.a);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A8: /* XOR B */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_A9: /* XOR C */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_AA: /* XOR D */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_AB: /* XOR E */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_AC: /* XOR H */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_AD: /* XOR L */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_AE: /* XOR (HL) */
		WGB_INSTR_XOR_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_AF: /* XOR A */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.a);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B0: /* OR B */
		WGB_INSTR_OR_R8(WGB_CPU_REG.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B1: /* OR C */
		WGB_INSTR_OR_R8(WGB_CPU_REG.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B2: /* OR D */
		WGB_INSTR_OR_R8(WGB_CPU_REG.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B3: /* OR E */
		WGB_INSTR_OR_R8(WGB_CPU_REG.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B4: /* OR H */
		WGB_INSTR_OR_R8(WGB_CPU_REG.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B5: /* OR L */
		WGB_INSTR_OR_R8(WGB_CPU_REG.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B6: /* OR (HL) */
		WGB_INSTR_OR_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B7: /* OR A */
		WGB_INSTR_OR_R8(WGB_CPU_REG.a);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B8: /* CP B */
		WGB_INSTR_CP_R8(WGB_CPU_REG.bc.bytes.b);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_B9: /* CP C */
		WGB_INSTR_CP_R8(WGB_CPU_REG.bc.bytes.c);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_BA: /* CP D */
		WGB_INSTR_CP_R8(WGB_CPU_REG.de.bytes.d);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_BB: /* CP E */
		WGB_INSTR_CP_R8(WGB_CPU_REG.de.bytes.e);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_BC: /* CP H */
		WGB_INSTR_CP_R8(WGB_CPU_REG.hl.bytes.h);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_BD: /* CP L */
		WGB_INSTR_CP_R8(WGB_CPU_REG.hl.bytes.l);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_BE: /* CP (HL) */
		WGB_INSTR_CP_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_BF: /* CP A */
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.z = 1;
		WGB_CPU_REG.f.f_bits.n = 1;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_C0: /* RET NZ */
		if(!WGB_CPU_REG.f.f_bits.z)
		{
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
        inst_cycles += 12;
		}
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_C1: /* POP BC */
    WGB_CPU_REG.bc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_C2: /* JP NZ, imm */
		if(!WGB_CPU_REG.f.f_bits.z)
		{		// best without 16-bit read when used in first half of dual fetch chain
        uint8_t c = (uint8_t)(oppair >> 8);
				WGB_CPU_REG.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
			inst_cycles += 4;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_C3: /* JP imm */
		{ // best without 16-bit read when used in first half of dual fetch chain
    uint8_t c = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
    uint8_t p = __gb_fetch8(gb);
    WGB_CPU_REG.pc.bytes.c = c;
    WGB_CPU_REG.pc.bytes.p = p;
		}
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_C4: /* CALL NZ imm */
		if(!WGB_CPU_REG.f.f_bits.z)
		{
			uint8_t c, p;
			c = (uint8_t)(oppair >> 8);
	    WGB_CPU_REG.pc.reg++;
			p = __gb_fetch8(gb);
			__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
			WGB_CPU_REG.pc.bytes.c = c;
			WGB_CPU_REG.pc.bytes.p = p;
			inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_C5: /* PUSH BC */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.bc.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_C6: /* ADD A, imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
		WGB_INSTR_ADC_R8(val, 0);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_C7: /* RST 0x0000 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0000;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_C8: /* RET Z */
		if(WGB_CPU_REG.f.f_bits.z)
		{
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
			inst_cycles += 12;
			opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		}
		else
		  opcode=(uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_C9: /* RET */
	{
    WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_CA: /* JP Z, imm */
		if(WGB_CPU_REG.f.f_bits.z)
		{
        uint8_t c = (uint8_t)(oppair >> 8);
				WGB_CPU_REG.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
			inst_cycles += 4;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_CB: /* CB INST */
		inst_cycles = __gb_execute_cb(gb);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg); // can cb change pc??? * revise
		//opcode = (uint8_t)(oppair >> 8); // things stopped working for megaman, rtype dx, etc with chaining when I made this change unless cgabmaactive was checked and opcode reloaded with gb_read
		goto threaded_dual_first_done;

first_CC: /* CALL Z, imm */
		if(WGB_CPU_REG.f.f_bits.z)
		{
			uint8_t c = (uint8_t)(oppair >> 8);
			WGB_CPU_REG.pc.reg++;
			uint8_t p = __gb_fetch8(gb);
			__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
			WGB_CPU_REG.pc.bytes.c = c;
			WGB_CPU_REG.pc.bytes.p = p;
			inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_CD: /* CALL imm */
	{
		uint8_t c = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		uint8_t p = __gb_fetch8(gb);
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.bytes.c = c;
		WGB_CPU_REG.pc.bytes.p = p;
	}
	opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
	goto threaded_dual_first_done;

first_CE: /* ADC A, imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
	  WGB_CPU_REG.pc.reg++;
		WGB_INSTR_ADC_R8(val, WGB_CPU_REG.f.f_bits.c);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_CF: /* RST 0x0008 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0008;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_D0: /* RET NC */
    if (!WGB_CPU_REG.f.f_bits.c)
    {
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
        inst_cycles += 12;
    }
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_D1: /* POP DE */
    WGB_CPU_REG.de.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_D2: /* JP NC, imm */
		if(!WGB_CPU_REG.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
        WGB_CPU_REG.pc.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg);
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
#endif
			inst_cycles += 4;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_D4: /* CALL NC, imm */
		if(!WGB_CPU_REG.f.f_bits.c)
		{
        uint8_t c =(uint8_t)(oppair >> 8);
				WGB_CPU_REG.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
				inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_D5: /* PUSH DE */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.de.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_D6: /* SUB imm */
	{
		uint8_t val =(uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		uint16_t temp = WGB_CPU_REG.a - val;
		WGB_CPU_REG.f.f_bits.z = ((temp & 0xFF) == 0x00);
		WGB_CPU_REG.f.f_bits.n = 1;
		WGB_CPU_REG.f.f_bits.h =
			(WGB_CPU_REG.a ^ val ^ temp) & 0x10 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
		WGB_CPU_REG.a = (temp & 0xFF);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_D7: /* RST 0x0010 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0010;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_D8: /* RET C */
		if(WGB_CPU_REG.f.f_bits.c)
		{
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
			inst_cycles += 12;
			opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		}
		else
			opcode = (uint8_t)(oppair >> 8);

		goto threaded_dual_first_done;

first_D9: /* RETI */
	{
    WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		gb->gb_ime = true;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
	}
	goto threaded_dual_first_done;

first_DA: /* JP C, imm */
		if(WGB_CPU_REG.f.f_bits.c)
		{
        uint8_t c = (uint8_t)(oppair >> 8);
				WGB_CPU_REG.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
				inst_cycles += 4;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_DC: /* CALL C, imm */
		if(WGB_CPU_REG.f.f_bits.c)
		{
        uint8_t c = (uint8_t)(oppair >> 8);
				WGB_CPU_REG.pc.reg++;
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
				inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_DE: /* SBC A, imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		WGB_INSTR_SBC_R8(val, WGB_CPU_REG.f.f_bits.c);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}
first_DF: /* RST 0x0018 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0018;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_E0: /* LD (0xFF00+imm), A */
	{
		const uint8_t io = (uint8_t)(oppair >> 8);
		if(WGB_LIKELY(io >= 0x80))
		{
			gb->hram_io[io] = WGB_CPU_REG.a;
		}
		else
			__gb_write(gb, 0xFF00 | io, WGB_CPU_REG.a);
		WGB_CPU_REG.pc.reg++;
	  opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_E1: /* POP HL */
    WGB_CPU_REG.hl.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_E2: /* LD (C), A */
		if(WGB_LIKELY(WGB_CPU_REG.bc.bytes.c >= 0x80))
		{
			gb->hram_io[WGB_CPU_REG.bc.bytes.c] = WGB_CPU_REG.a;
		}
		else
			__gb_write(gb, 0xFF00 | WGB_CPU_REG.bc.bytes.c, WGB_CPU_REG.a);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES_DISABLED
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#else
		opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;

first_E5: /* PUSH HL */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.hl.reg);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_E6: /* AND imm */
	{
		uint8_t temp = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		WGB_INSTR_AND_R8(temp);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_E7: /* RST 0x0020 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0020;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_E8: /* ADD SP, imm */
	{
		int8_t offset = (int8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = ((WGB_CPU_REG.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
		WGB_CPU_REG.sp.reg += offset;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_E9: /* JP (HL) */
		WGB_CPU_REG.pc.reg = WGB_CPU_REG.hl.reg;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_EA: /* LD (imm), A */
	{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
    uint8_t l = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		oppair = __gb_read16(gb, WGB_CPU_REG.pc.reg++);
    uint8_t h = oppair; // autotruncate
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
		__gb_write(gb, addr, WGB_CPU_REG.a);
		opcode = oppair >> 8;
#else
    uint8_t l = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
		__gb_write(gb, addr, WGB_CPU_REG.a);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#endif
		goto threaded_dual_first_done;
	}

first_EE: /* XOR imm */
		WGB_INSTR_XOR_R8((uint8_t)(oppair >> 8));
		WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_EF: /* RST 0x0028 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0028;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_F0: /* LD A, (0xFF00+imm) */
	{
		const uint8_t io = (uint8_t)(oppair >> 8);
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(io == IO_STAT)
			WGB_CPU_REG.a = __gb_fast_stat_poll_read(gb,
				(uint16_t)(WGB_CPU_REG.pc.reg - 1),
				(uint16_t)(WGB_CPU_REG.pc.reg + 1));
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, io, &WGB_CPU_REG.a))
				WGB_CPU_REG.a = __gb_read(gb, 0xFF00 | io);
		WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_F1: /* POP AF */
	{
		const uint16_t temp_16 =
			__gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		uint8_t temp_8 = (uint8_t)temp_16;
		WGB_CPU_REG.f.f_bits.z = (temp_8 >> 7) & 1;
		WGB_CPU_REG.f.f_bits.n = (temp_8 >> 6) & 1;
		WGB_CPU_REG.f.f_bits.h = (temp_8 >> 5) & 1;
		WGB_CPU_REG.f.f_bits.c = (temp_8 >> 4) & 1;
		WGB_CPU_REG.a = (uint8_t)(temp_16 >> 8);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;
	}

first_F2: /* LD A, (C) */
	{
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
		if(WGB_CPU_REG.bc.bytes.c == IO_STAT)
			WGB_CPU_REG.a = __gb_fast_stat_poll_c_read(gb,
				(uint16_t)(WGB_CPU_REG.pc.reg - 1), WGB_CPU_REG.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, WGB_CPU_REG.bc.bytes.c, &WGB_CPU_REG.a))
				WGB_CPU_REG.a = __gb_read(gb, 0xFF00 | WGB_CPU_REG.bc.bytes.c);
#if WALNUT_GB_SAFE_DUALFETCH_OPCODES
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#else
	opcode = (uint8_t)(oppair >> 8);
#endif
		goto threaded_dual_first_done;
	}

first_F3: /* DI */
		gb->gb_ime = false;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_F5: /* PUSH AF */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg,
			((uint16_t)WGB_CPU_REG.a << 8) |
			WGB_CPU_REG.f.f_bits.z << 7 | WGB_CPU_REG.f.f_bits.n << 6 |
			WGB_CPU_REG.f.f_bits.h << 5 | WGB_CPU_REG.f.f_bits.c << 4);
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_F6: /* OR imm */
		WGB_INSTR_OR_R8((uint8_t)(oppair >> 8));
		WGB_CPU_REG.pc.reg++;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_F7: /* PUSH AF */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0030;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_F8: /* LD HL, SP+/-imm */
	{
		/* Taken from SameBoy, which is released under MIT Licence. */
		int8_t offset = (int8_t) (oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		WGB_CPU_REG.hl.reg = WGB_CPU_REG.sp.reg + offset;
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = ((WGB_CPU_REG.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_F9: /* LD SP, HL */
		WGB_CPU_REG.sp.reg = WGB_CPU_REG.hl.reg;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_FA: /* LD A, (imm) */
	{
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
    uint8_t l = (uint8_t)(oppair >> 8); // we dont increment pc because a interrupt might change it an invalidate this byte, we increment if no interrupt between 1st and 2nd opcode handlers
		WGB_CPU_REG.pc.reg++;
		oppair = __gb_read16(gb, WGB_CPU_REG.pc.reg++);
    uint8_t h = oppair; // auto-truncate
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#else
    uint8_t l = (uint8_t)(oppair >> 8); // we dont increment pc because a interrupt might change it an invalidate this byte, we increment if no interrupt between 1st and 2nd opcode handlers
		WGB_CPU_REG.pc.reg++;
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(addr == (uint16_t)(IO_ADDR | IO_STAT))
			WGB_CPU_REG.a = __gb_fast_stat_poll_read(gb,
				(uint16_t)(WGB_CPU_REG.pc.reg - 3), WGB_CPU_REG.pc.reg);
		else
#endif
			if(!__gb_fast_abs_read_hot(gb, addr, &WGB_CPU_REG.a))
				WGB_CPU_REG.a = __gb_read(gb, addr);
#if WALNUT_GB_16_BIT_OPS_DUALFETCH_DISABLED
		opcode = oppair >> 8;
#else
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
#endif
		goto threaded_dual_first_done;
	}

first_FB: /* EI */
		gb->gb_ime = true;
		opcode = (uint8_t)(oppair >> 8);
		goto threaded_dual_first_done;

first_FE: /* CP imm */
	{
		uint8_t val = (uint8_t)(oppair >> 8);
		WGB_CPU_REG.pc.reg++;
		WGB_INSTR_CP_R8(val);
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;
	}

first_FF: /* RST 0x0038 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0038;
		opcode = __gb_read(gb, WGB_CPU_REG.pc.reg);
		goto threaded_dual_first_done;

first_invalid:
		/* Return address where invalid opcode that was read. */
	(gb->gb_error)(gb, GB_INVALID_OPCODE, WGB_CPU_REG.pc.reg - 1);
	/* The front-end error handler returns; leave through the normal exit
	 * so runner-local state (lazy post pointer) is torn down. */
	goto threaded_dual_exit;
