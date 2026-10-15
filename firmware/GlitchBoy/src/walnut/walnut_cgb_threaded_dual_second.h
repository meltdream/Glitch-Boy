/* Generated mechanically from walnut_cgb.h dualfetch second opcode switch. */
second_00: /* NOP */
		goto threaded_dual_second_done;

second_01: /* LD BC, imm */
#if WALNUT_GB_16_BIT_OPS
    WGB_CPU_REG.bc.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2;
#else
    WGB_CPU_REG.bc.bytes.c = __gb_fetch8(gb);
    WGB_CPU_REG.bc.bytes.b = __gb_fetch8(gb);
#endif
		goto threaded_dual_second_done;
second_02: /* LD (BC), A */
		__gb_write(gb, WGB_CPU_REG.bc.reg, WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_03: /* INC BC */
		WGB_CPU_REG.bc.reg++;
		goto threaded_dual_second_done;

second_04: /* INC B */
		WGB_INSTR_INC_R8(WGB_CPU_REG.bc.bytes.b);
		goto threaded_dual_second_done;

second_05: /* DEC B */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.bc.bytes.b);
		goto threaded_dual_second_done;

second_06: /* LD B, imm */
		WGB_CPU_REG.bc.bytes.b = __gb_fetch8(gb);
		goto threaded_dual_second_done;

second_07: /* RLCA */
		WGB_CPU_REG.a = (WGB_CPU_REG.a << 1) | (WGB_CPU_REG.a >> 7);
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = (WGB_CPU_REG.a & 0x01);
		goto threaded_dual_second_done;

second_08: /* LD (imm), SP */
	{
#if WALNUT_GB_16_BIT_OPS
    uint16_t addr = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2;
    __gb_write(gb, addr++, WGB_CPU_REG.sp.bytes.p);
    __gb_write(gb, addr, WGB_CPU_REG.sp.bytes.s);
#else
    uint8_t l = __gb_fetch8(gb);
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
    __gb_write(gb, addr++, WGB_CPU_REG.sp.bytes.p);
    __gb_write(gb, addr, WGB_CPU_REG.sp.bytes.s);
#endif
    goto threaded_dual_second_done;
	}

second_09: /* ADD HL, BC */
	{
		uint_fast32_t temp = WGB_CPU_REG.hl.reg + WGB_CPU_REG.bc.reg;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h =
			(temp ^ WGB_CPU_REG.hl.reg ^ WGB_CPU_REG.bc.reg) & 0x1000 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		WGB_CPU_REG.hl.reg = (temp & 0x0000FFFF);
		goto threaded_dual_second_done;
	}

second_0A: /* LD A, (BC) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.bc.reg);
		goto threaded_dual_second_done;

second_0B: /* DEC BC */
		WGB_CPU_REG.bc.reg--;
		goto threaded_dual_second_done;

second_0C: /* INC C */
		WGB_INSTR_INC_R8(WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;

second_0D: /* DEC C */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;

second_0E: /* LD C, imm */
		WGB_CPU_REG.bc.bytes.c = __gb_fetch8(gb);
		goto threaded_dual_second_done;

second_0F: /* RRCA */
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = WGB_CPU_REG.a & 0x01;
		WGB_CPU_REG.a = (WGB_CPU_REG.a >> 1) | (WGB_CPU_REG.a << 7);
		goto threaded_dual_second_done;

second_10: /* STOP */
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
		/* STOP is two bytes; skip the padding byte. */
		WGB_CPU_REG.pc.reg++;
		goto threaded_dual_second_done;

second_11: /* LD DE, imm */
#if WALNUT_GB_16_BIT_OPS
    WGB_CPU_REG.de.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2;
#else
    WGB_CPU_REG.de.bytes.e = __gb_fetch8(gb);
    WGB_CPU_REG.de.bytes.d = __gb_fetch8(gb);
#endif
		goto threaded_dual_second_done;

second_12: /* LD (DE), A */
		__gb_write(gb, WGB_CPU_REG.de.reg, WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_13: /* INC DE */
		WGB_CPU_REG.de.reg++;
		goto threaded_dual_second_done;

second_14: /* INC D */
		WGB_INSTR_INC_R8(WGB_CPU_REG.de.bytes.d);
		goto threaded_dual_second_done;

second_15: /* DEC D */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.de.bytes.d);
		goto threaded_dual_second_done;

second_16: /* LD D, imm */
		WGB_CPU_REG.de.bytes.d = __gb_fetch8(gb);
		goto threaded_dual_second_done;

second_17: /* RLA */
	{
		uint8_t temp = WGB_CPU_REG.a;
		WGB_CPU_REG.a = (WGB_CPU_REG.a << 1) | WGB_CPU_REG.f.f_bits.c;
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = (temp >> 7) & 0x01;
		goto threaded_dual_second_done;
	}

second_18: /* JR imm */
	{
		int8_t temp = (int8_t) __gb_fetch8(gb);
		WGB_CPU_REG.pc.reg += temp;
		goto threaded_dual_second_done;
	}

second_19: /* ADD HL, DE */
	{
		uint_fast32_t temp = WGB_CPU_REG.hl.reg + WGB_CPU_REG.de.reg;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h =
			(temp ^ WGB_CPU_REG.hl.reg ^ WGB_CPU_REG.de.reg) & 0x1000 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFFFF0000) ? 1 : 0;
		WGB_CPU_REG.hl.reg = (temp & 0x0000FFFF);
		goto threaded_dual_second_done;
	}

second_1A: /* LD A, (DE) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.de.reg);
		goto threaded_dual_second_done;

second_1B: /* DEC DE */
		WGB_CPU_REG.de.reg--;
		goto threaded_dual_second_done;

second_1C: /* INC E */
		WGB_INSTR_INC_R8(WGB_CPU_REG.de.bytes.e);
		goto threaded_dual_second_done;

second_1D: /* DEC E */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.de.bytes.e);
		goto threaded_dual_second_done;

second_1E: /* LD E, imm */
		WGB_CPU_REG.de.bytes.e = __gb_fetch8(gb);
		goto threaded_dual_second_done;

second_1F: /* RRA */
	{
		uint8_t temp = WGB_CPU_REG.a;
		WGB_CPU_REG.a = WGB_CPU_REG.a >> 1 | (WGB_CPU_REG.f.f_bits.c << 7);
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.c = temp & 0x1;
		goto threaded_dual_second_done;
	}

second_20: /* JR NZ, imm */
		if(!WGB_CPU_REG.f.f_bits.z)
		{
			const uint16_t branch_pc = (uint16_t)(WGB_CPU_REG.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg, branch_pc, 0x20);
		}
		else
			WGB_CPU_REG.pc.reg++;

		goto threaded_dual_second_done;

second_21: /* LD HL, imm */
#if WALNUT_GB_16_BIT_OPS
    WGB_CPU_REG.hl.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2;
#else
    WGB_CPU_REG.hl.bytes.l = __gb_fetch8(gb);
    WGB_CPU_REG.hl.bytes.h = __gb_fetch8(gb);
#endif
		goto threaded_dual_second_done;

second_22: /* LDI (HL), A */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.a);
		WGB_CPU_REG.hl.reg++;
		goto threaded_dual_second_done;

second_23: /* INC HL */
		WGB_CPU_REG.hl.reg++;
		goto threaded_dual_second_done;

second_24: /* INC H */
		WGB_INSTR_INC_R8(WGB_CPU_REG.hl.bytes.h);
		goto threaded_dual_second_done;

second_25: /* DEC H */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.hl.bytes.h);
		goto threaded_dual_second_done;

second_26: /* LD H, imm */
		WGB_CPU_REG.hl.bytes.h = __gb_fetch8(gb);
		goto threaded_dual_second_done;

second_27: /* DAA */
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

		goto threaded_dual_second_done;
	}

second_28: /* JR Z, imm */
		if(WGB_CPU_REG.f.f_bits.z)
		{
			const uint16_t branch_pc = (uint16_t)(WGB_CPU_REG.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
			__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg, branch_pc, 0x28);
		}
		else
			WGB_CPU_REG.pc.reg++;

		goto threaded_dual_second_done;

second_29: /* ADD HL, HL */
	{
		WGB_CPU_REG.f.f_bits.c = (WGB_CPU_REG.hl.reg & 0x8000) > 0;
		WGB_CPU_REG.hl.reg <<= 1;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h = (WGB_CPU_REG.hl.reg & 0x1000) > 0;
		goto threaded_dual_second_done;
	}

second_2A: /* LD A, (HL+) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.hl.reg++);
		goto threaded_dual_second_done;

second_2B: /* DEC HL */
		WGB_CPU_REG.hl.reg--;
		goto threaded_dual_second_done;

second_2C: /* INC L */
		WGB_INSTR_INC_R8(WGB_CPU_REG.hl.bytes.l);
		goto threaded_dual_second_done;

second_2D: /* DEC L */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.hl.bytes.l);
		goto threaded_dual_second_done;

second_2E: /* LD L, imm */
		WGB_CPU_REG.hl.bytes.l = __gb_fetch8(gb);
		goto threaded_dual_second_done;

second_2F: /* CPL */
		WGB_CPU_REG.a = ~WGB_CPU_REG.a;
		WGB_CPU_REG.f.f_bits.n = 1;
		WGB_CPU_REG.f.f_bits.h = 1;
		goto threaded_dual_second_done;

second_30: /* JR NC, imm */
		if(!WGB_CPU_REG.f.f_bits.c)
		{
			const uint16_t branch_pc = (uint16_t)(WGB_CPU_REG.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					WGB_CPU_REG.pc.reg, branch_pc))
				__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg,
					branch_pc, 0x30);
#endif
		}
		else
			WGB_CPU_REG.pc.reg++;

		goto threaded_dual_second_done;

second_31: /* LD SP, imm */
#if WALNUT_GB_16_BIT_OPS
    WGB_CPU_REG.sp.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2;
#else
    WGB_CPU_REG.sp.bytes.p = __gb_fetch8(gb);
    WGB_CPU_REG.sp.bytes.s = __gb_fetch8(gb);
#endif
		goto threaded_dual_second_done;

second_32: /* LD (HL), A */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.a);
		WGB_CPU_REG.hl.reg--;
		goto threaded_dual_second_done;

second_33: /* INC SP */
		WGB_CPU_REG.sp.reg++;
		goto threaded_dual_second_done;

second_34: /* INC (HL) */
	{
		uint8_t temp = __gb_read(gb, WGB_CPU_REG.hl.reg);
		WGB_INSTR_INC_R8(temp);
		__gb_write(gb, WGB_CPU_REG.hl.reg, temp);
		goto threaded_dual_second_done;
	}

second_35: /* DEC (HL) */
	{
		uint8_t temp = __gb_read(gb, WGB_CPU_REG.hl.reg);
		WGB_INSTR_DEC_R8(temp);
		__gb_write(gb, WGB_CPU_REG.hl.reg, temp);
		goto threaded_dual_second_done;
	}

second_36: /* LD (HL), imm */
		__gb_write(gb, WGB_CPU_REG.hl.reg, __gb_fetch8(gb));
		goto threaded_dual_second_done;

second_37: /* SCF */
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h = 0;
		WGB_CPU_REG.f.f_bits.c = 1;
		goto threaded_dual_second_done;

second_38: /* JR C, imm */
		if(WGB_CPU_REG.f.f_bits.c)
		{
			const uint16_t branch_pc = (uint16_t)(WGB_CPU_REG.pc.reg - 1);
			int8_t temp = (int8_t) __gb_fetch8(gb);
			WGB_CPU_REG.pc.reg += temp;
			inst_cycles += 4;
#if WALNUT_GB_FAST_POLL_WAIT_ACTIVE
			if(__gb_fast_poll_wait_carry_ly_prefilter(gb,
					WGB_CPU_REG.pc.reg, branch_pc))
				__gb_fast_poll_wait_maybe(gb, WGB_CPU_REG.pc.reg,
					branch_pc, 0x38);
#endif
		}
		else
			WGB_CPU_REG.pc.reg++;

		goto threaded_dual_second_done;

second_39: /* ADD HL, SP */
	{
		uint_fast32_t temp = WGB_CPU_REG.hl.reg + WGB_CPU_REG.sp.reg;
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h =
			((WGB_CPU_REG.hl.reg & 0xFFF) + (WGB_CPU_REG.sp.reg & 0xFFF)) & 0x1000 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = temp & 0x10000 ? 1 : 0;
		WGB_CPU_REG.hl.reg = (uint16_t)temp;
		goto threaded_dual_second_done;
	}

second_3A: /* LD A, (HL) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.hl.reg--);
		goto threaded_dual_second_done;

second_3B: /* DEC SP */
		WGB_CPU_REG.sp.reg--;
		goto threaded_dual_second_done;

second_3C: /* INC A */
		WGB_INSTR_INC_R8(WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_3D: /* DEC A */
		WGB_INSTR_DEC_R8(WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_3E: /* LD A, imm */
		WGB_CPU_REG.a = __gb_fetch8(gb);
		goto threaded_dual_second_done;

second_3F: /* CCF */
		WGB_CPU_REG.f.f_bits.n = 0;
		WGB_CPU_REG.f.f_bits.h = 0;
		WGB_CPU_REG.f.f_bits.c = ~WGB_CPU_REG.f.f_bits.c;
		goto threaded_dual_second_done;

second_40: /* LD B, B */
		goto threaded_dual_second_done;

second_41: /* LD B, C */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.bc.bytes.c;
		goto threaded_dual_second_done;

second_42: /* LD B, D */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.de.bytes.d;
		goto threaded_dual_second_done;

second_43: /* LD B, E */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.de.bytes.e;
		goto threaded_dual_second_done;

second_44: /* LD B, H */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.hl.bytes.h;
		goto threaded_dual_second_done;

second_45: /* LD B, L */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.hl.bytes.l;
		goto threaded_dual_second_done;

second_46: /* LD B, (HL) */
		WGB_CPU_REG.bc.bytes.b = __gb_read(gb, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_47: /* LD B, A */
		WGB_CPU_REG.bc.bytes.b = WGB_CPU_REG.a;
		goto threaded_dual_second_done;

second_48: /* LD C, B */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.bc.bytes.b;
		goto threaded_dual_second_done;

second_49: /* LD C, C */
		goto threaded_dual_second_done;

second_4A: /* LD C, D */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.de.bytes.d;
		goto threaded_dual_second_done;

second_4B: /* LD C, E */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.de.bytes.e;
		goto threaded_dual_second_done;

second_4C: /* LD C, H */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.hl.bytes.h;
		goto threaded_dual_second_done;

second_4D: /* LD C, L */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.hl.bytes.l;
		goto threaded_dual_second_done;

second_4E: /* LD C, (HL) */
		WGB_CPU_REG.bc.bytes.c = __gb_read(gb, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_4F: /* LD C, A */
		WGB_CPU_REG.bc.bytes.c = WGB_CPU_REG.a;
		goto threaded_dual_second_done;

second_50: /* LD D, B */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.bc.bytes.b;
		goto threaded_dual_second_done;

second_51: /* LD D, C */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.bc.bytes.c;
		goto threaded_dual_second_done;

second_52: /* LD D, D */
		goto threaded_dual_second_done;

second_53: /* LD D, E */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.de.bytes.e;
		goto threaded_dual_second_done;

second_54: /* LD D, H */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.hl.bytes.h;
		goto threaded_dual_second_done;

second_55: /* LD D, L */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.hl.bytes.l;
		goto threaded_dual_second_done;

second_56: /* LD D, (HL) */
		WGB_CPU_REG.de.bytes.d = __gb_read(gb, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_57: /* LD D, A */
		WGB_CPU_REG.de.bytes.d = WGB_CPU_REG.a;
		goto threaded_dual_second_done;

second_58: /* LD E, B */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.bc.bytes.b;
		goto threaded_dual_second_done;

second_59: /* LD E, C */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.bc.bytes.c;
		goto threaded_dual_second_done;

second_5A: /* LD E, D */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.de.bytes.d;
		goto threaded_dual_second_done;

second_5B: /* LD E, E */
		goto threaded_dual_second_done;

second_5C: /* LD E, H */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.hl.bytes.h;
		goto threaded_dual_second_done;

second_5D: /* LD E, L */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.hl.bytes.l;
		goto threaded_dual_second_done;

second_5E: /* LD E, (HL) */
		WGB_CPU_REG.de.bytes.e = __gb_read(gb, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_5F: /* LD E, A */
		WGB_CPU_REG.de.bytes.e = WGB_CPU_REG.a;
		goto threaded_dual_second_done;

second_60: /* LD H, B */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.bc.bytes.b;
		goto threaded_dual_second_done;

second_61: /* LD H, C */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.bc.bytes.c;
		goto threaded_dual_second_done;

second_62: /* LD H, D */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.de.bytes.d;
		goto threaded_dual_second_done;

second_63: /* LD H, E */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.de.bytes.e;
		goto threaded_dual_second_done;

second_64: /* LD H, H */
		goto threaded_dual_second_done;

second_65: /* LD H, L */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.hl.bytes.l;
		goto threaded_dual_second_done;

second_66: /* LD H, (HL) */
		WGB_CPU_REG.hl.bytes.h = __gb_read(gb, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_67: /* LD H, A */
		WGB_CPU_REG.hl.bytes.h = WGB_CPU_REG.a;
		goto threaded_dual_second_done;

second_68: /* LD L, B */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.bc.bytes.b;
		goto threaded_dual_second_done;

second_69: /* LD L, C */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.bc.bytes.c;
		goto threaded_dual_second_done;

second_6A: /* LD L, D */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.de.bytes.d;
		goto threaded_dual_second_done;

second_6B: /* LD L, E */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.de.bytes.e;
		goto threaded_dual_second_done;

second_6C: /* LD L, H */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.hl.bytes.h;
		goto threaded_dual_second_done;

second_6D: /* LD L, L */
		goto threaded_dual_second_done;

second_6E: /* LD L, (HL) */
		WGB_CPU_REG.hl.bytes.l = __gb_read(gb, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_6F: /* LD L, A */
		WGB_CPU_REG.hl.bytes.l = WGB_CPU_REG.a;
		goto threaded_dual_second_done;

second_70: /* LD (HL), B */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.bc.bytes.b);
		goto threaded_dual_second_done;

second_71: /* LD (HL), C */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;

second_72: /* LD (HL), D */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.de.bytes.d);
		goto threaded_dual_second_done;

second_73: /* LD (HL), E */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.de.bytes.e);
		goto threaded_dual_second_done;

second_74: /* LD (HL), H */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.hl.bytes.h);
		goto threaded_dual_second_done;

second_75: /* LD (HL), L */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.hl.bytes.l);
		goto threaded_dual_second_done;

second_76: /* HALT */
	{
		/* TODO: Emulate HALT bug? */
		gb->gb_halt = true;

		if(__gb_halt_fast_forward_to_vblank(gb))
		{
			inst_cycles = 0;
			goto threaded_dual_second_done;
		}
		goto threaded_dual_second_done;
	}

second_77: /* LD (HL), A */
		__gb_write(gb, WGB_CPU_REG.hl.reg, WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_78: /* LD A, B */
		WGB_CPU_REG.a = WGB_CPU_REG.bc.bytes.b;
		goto threaded_dual_second_done;

second_79: /* LD A, C */
		WGB_CPU_REG.a = WGB_CPU_REG.bc.bytes.c;
		goto threaded_dual_second_done;

second_7A: /* LD A, D */
		WGB_CPU_REG.a = WGB_CPU_REG.de.bytes.d;
		goto threaded_dual_second_done;

second_7B: /* LD A, E */
		WGB_CPU_REG.a = WGB_CPU_REG.de.bytes.e;
		goto threaded_dual_second_done;

second_7C: /* LD A, H */
		WGB_CPU_REG.a = WGB_CPU_REG.hl.bytes.h;
		goto threaded_dual_second_done;

second_7D: /* LD A, L */
		WGB_CPU_REG.a = WGB_CPU_REG.hl.bytes.l;
		goto threaded_dual_second_done;

second_7E: /* LD A, (HL) */
		WGB_CPU_REG.a = __gb_read(gb, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_7F: /* LD A, A */
		goto threaded_dual_second_done;

second_80: /* ADD A, B */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.b, 0);
		goto threaded_dual_second_done;

second_81: /* ADD A, C */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.c, 0);
		goto threaded_dual_second_done;

second_82: /* ADD A, D */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.d, 0);
		goto threaded_dual_second_done;

second_83: /* ADD A, E */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.e, 0);
		goto threaded_dual_second_done;

second_84: /* ADD A, H */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.h, 0);
		goto threaded_dual_second_done;

second_85: /* ADD A, L */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.l, 0);
		goto threaded_dual_second_done;

second_86: /* ADD A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), 0);
		goto threaded_dual_second_done;

second_87: /* ADD A, A */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.a, 0);
		goto threaded_dual_second_done;

second_88: /* ADC A, B */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.b, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_89: /* ADC A, C */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.bc.bytes.c, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_8A: /* ADC A, D */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.d, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_8B: /* ADC A, E */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.de.bytes.e, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_8C: /* ADC A, H */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.h, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_8D: /* ADC A, L */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.hl.bytes.l, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_8E: /* ADC A, (HL) */
		WGB_INSTR_ADC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_8F: /* ADC A, A */
		WGB_INSTR_ADC_R8(WGB_CPU_REG.a, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_90: /* SUB B */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.b, 0);
		goto threaded_dual_second_done;

second_91: /* SUB C */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.c, 0);
		goto threaded_dual_second_done;

second_92: /* SUB D */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.d, 0);
		goto threaded_dual_second_done;

second_93: /* SUB E */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.e, 0);
		goto threaded_dual_second_done;

second_94: /* SUB H */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.h, 0);
		goto threaded_dual_second_done;

second_95: /* SUB L */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.l, 0);
		goto threaded_dual_second_done;

second_96: /* SUB (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), 0);
		goto threaded_dual_second_done;

second_97: /* SUB A */
		WGB_CPU_REG.a = 0;
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.z = 1;
		WGB_CPU_REG.f.f_bits.n = 1;
		goto threaded_dual_second_done;

second_98: /* SBC A, B */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.b, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_99: /* SBC A, C */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.bc.bytes.c, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_9A: /* SBC A, D */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.d, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_9B: /* SBC A, E */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.de.bytes.e, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_9C: /* SBC A, H */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.h, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_9D: /* SBC A, L */
		WGB_INSTR_SBC_R8(WGB_CPU_REG.hl.bytes.l, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_9E: /* SBC A, (HL) */
		WGB_INSTR_SBC_R8(__gb_read(gb, WGB_CPU_REG.hl.reg), WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;

second_9F: /* SBC A, A */
		WGB_CPU_REG.a = WGB_CPU_REG.f.f_bits.c ? 0xFF : 0x00;
		WGB_CPU_REG.f.f_bits.z = !WGB_CPU_REG.f.f_bits.c;
		WGB_CPU_REG.f.f_bits.n = 1;
		WGB_CPU_REG.f.f_bits.h = WGB_CPU_REG.f.f_bits.c;
		goto threaded_dual_second_done;

second_A0: /* AND B */
		WGB_INSTR_AND_R8(WGB_CPU_REG.bc.bytes.b);
		goto threaded_dual_second_done;

second_A1: /* AND C */
		WGB_INSTR_AND_R8(WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;

second_A2: /* AND D */
		WGB_INSTR_AND_R8(WGB_CPU_REG.de.bytes.d);
		goto threaded_dual_second_done;

second_A3: /* AND E */
		WGB_INSTR_AND_R8(WGB_CPU_REG.de.bytes.e);
		goto threaded_dual_second_done;

second_A4: /* AND H */
		WGB_INSTR_AND_R8(WGB_CPU_REG.hl.bytes.h);
		goto threaded_dual_second_done;

second_A5: /* AND L */
		WGB_INSTR_AND_R8(WGB_CPU_REG.hl.bytes.l);
		goto threaded_dual_second_done;

second_A6: /* AND (HL) */
		WGB_INSTR_AND_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		goto threaded_dual_second_done;

second_A7: /* AND A */
		WGB_INSTR_AND_R8(WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_A8: /* XOR B */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.bc.bytes.b);
		goto threaded_dual_second_done;

second_A9: /* XOR C */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;

second_AA: /* XOR D */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.de.bytes.d);
		goto threaded_dual_second_done;

second_AB: /* XOR E */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.de.bytes.e);
		goto threaded_dual_second_done;

second_AC: /* XOR H */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.hl.bytes.h);
		goto threaded_dual_second_done;

second_AD: /* XOR L */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.hl.bytes.l);
		goto threaded_dual_second_done;

second_AE: /* XOR (HL) */
		WGB_INSTR_XOR_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		goto threaded_dual_second_done;

second_AF: /* XOR A */
		WGB_INSTR_XOR_R8(WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_B0: /* OR B */
		WGB_INSTR_OR_R8(WGB_CPU_REG.bc.bytes.b);
		goto threaded_dual_second_done;

second_B1: /* OR C */
		WGB_INSTR_OR_R8(WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;

second_B2: /* OR D */
		WGB_INSTR_OR_R8(WGB_CPU_REG.de.bytes.d);
		goto threaded_dual_second_done;

second_B3: /* OR E */
		WGB_INSTR_OR_R8(WGB_CPU_REG.de.bytes.e);
		goto threaded_dual_second_done;

second_B4: /* OR H */
		WGB_INSTR_OR_R8(WGB_CPU_REG.hl.bytes.h);
		goto threaded_dual_second_done;

second_B5: /* OR L */
		WGB_INSTR_OR_R8(WGB_CPU_REG.hl.bytes.l);
		goto threaded_dual_second_done;

second_B6: /* OR (HL) */
		WGB_INSTR_OR_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		goto threaded_dual_second_done;

second_B7: /* OR A */
		WGB_INSTR_OR_R8(WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_B8: /* CP B */
		WGB_INSTR_CP_R8(WGB_CPU_REG.bc.bytes.b);
		goto threaded_dual_second_done;

second_B9: /* CP C */
		WGB_INSTR_CP_R8(WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;

second_BA: /* CP D */
		WGB_INSTR_CP_R8(WGB_CPU_REG.de.bytes.d);
		goto threaded_dual_second_done;

second_BB: /* CP E */
		WGB_INSTR_CP_R8(WGB_CPU_REG.de.bytes.e);
		goto threaded_dual_second_done;

second_BC: /* CP H */
		WGB_INSTR_CP_R8(WGB_CPU_REG.hl.bytes.h);
		goto threaded_dual_second_done;

second_BD: /* CP L */
		WGB_INSTR_CP_R8(WGB_CPU_REG.hl.bytes.l);
		goto threaded_dual_second_done;

second_BE: /* CP (HL) */
		WGB_INSTR_CP_R8(__gb_read(gb, WGB_CPU_REG.hl.reg));
		goto threaded_dual_second_done;

second_BF: /* CP A */
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.z = 1;
		WGB_CPU_REG.f.f_bits.n = 1;
		goto threaded_dual_second_done;

second_C0: /* RET NZ */
		if(!WGB_CPU_REG.f.f_bits.z)
		{
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
        inst_cycles += 12;
		}

		goto threaded_dual_second_done;

second_C1: /* POP BC */
    WGB_CPU_REG.bc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		goto threaded_dual_second_done;

second_C2: /* JP NZ, imm */
		if(!WGB_CPU_REG.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
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

		goto threaded_dual_second_done;

second_C3: /* JP imm */
#if WALNUT_GB_16_BIT_OPS
    WGB_CPU_REG.pc.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg);
#else
{
    uint8_t c = __gb_fetch8(gb);
    uint8_t p = __gb_fetch8(gb);
    WGB_CPU_REG.pc.bytes.c = c;
    WGB_CPU_REG.pc.bytes.p = p;
}
#endif
		goto threaded_dual_second_done;

second_C4: /* CALL NZ imm */
		if(!WGB_CPU_REG.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
			uint16_t addr = __gb_read16(gb, WGB_CPU_REG.pc.reg);
			WGB_CPU_REG.pc.reg += 2;
			__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
			WGB_CPU_REG.pc.reg = addr;
#else
			uint8_t c, p;
			c = __gb_fetch8(gb);
			p = __gb_fetch8(gb);
			__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
			WGB_CPU_REG.pc.bytes.c = c;
			WGB_CPU_REG.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;

		goto threaded_dual_second_done;

second_C5: /* PUSH BC */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.bc.reg);
		goto threaded_dual_second_done;

second_C6: /* ADD A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_ADC_R8(val, 0);
		goto threaded_dual_second_done;
	}

second_C7: /* RST 0x0000 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0000;
		goto threaded_dual_second_done;

second_C8: /* RET Z */
		if(WGB_CPU_REG.f.f_bits.z)
		{
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
			inst_cycles += 12;
		}
		goto threaded_dual_second_done;

second_C9: /* RET */
	{
    WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		goto threaded_dual_second_done;
	}

second_CA: /* JP Z, imm */
		if(WGB_CPU_REG.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
        WGB_CPU_REG.pc.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg); // no need to advance pc because of jump
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

		goto threaded_dual_second_done;

second_CB: /* CB INST */
		inst_cycles = __gb_execute_cb(gb);
		goto threaded_dual_second_done;

second_CC: /* CALL Z, imm */
		if(WGB_CPU_REG.f.f_bits.z)
		{
#if WALNUT_GB_16_BIT_OPS
        uint16_t addr = __gb_read16(gb, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.reg += 2; // advance past immediate

        // push old PC
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);

        WGB_CPU_REG.pc.reg = addr;
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;

		goto threaded_dual_second_done;

second_CD: /* CALL imm */
#if WALNUT_GB_16_BIT_OPS
{
    uint16_t addr = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2; // advance past immediate
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg = addr;
}
#else
{
    uint8_t c = __gb_fetch8(gb);
    uint8_t p = __gb_fetch8(gb);
    __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.bytes.c = c;
    WGB_CPU_REG.pc.bytes.p = p;
}
#endif
	goto threaded_dual_second_done;

second_CE: /* ADC A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_ADC_R8(val, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;
	}

second_CF: /* RST 0x0008 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0008;
		goto threaded_dual_second_done;

second_D0: /* RET NC */

    if (!WGB_CPU_REG.f.f_bits.c)
    {
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
        inst_cycles += 12;
    }

		goto threaded_dual_second_done;

second_D1: /* POP DE */
    WGB_CPU_REG.de.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		goto threaded_dual_second_done;

second_D2: /* JP NC, imm */
		if(!WGB_CPU_REG.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_OPS
        WGB_CPU_REG.pc.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg); // jump so no need to advance pc
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

		goto threaded_dual_second_done;

second_D4: /* CALL NC, imm */
		if(!WGB_CPU_REG.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT_OPS
        uint16_t addr = __gb_read16(gb, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.reg += 2; // advance PC past immediate
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.reg = addr;
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;

		goto threaded_dual_second_done;

second_D5: /* PUSH DE */
	// this was revised but working backwards from this 16-bit op to find the one thats broken
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.de.reg);
		goto threaded_dual_second_done;

second_D6: /* SUB imm */
	{
		uint8_t val = __gb_fetch8(gb);
		uint16_t temp = WGB_CPU_REG.a - val;
		WGB_CPU_REG.f.f_bits.z = ((temp & 0xFF) == 0x00);
		WGB_CPU_REG.f.f_bits.n = 1;
		WGB_CPU_REG.f.f_bits.h =
			(WGB_CPU_REG.a ^ val ^ temp) & 0x10 ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = (temp & 0xFF00) ? 1 : 0;
		WGB_CPU_REG.a = (temp & 0xFF);
		goto threaded_dual_second_done;
	}

second_D7: /* RST 0x0010 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0010;
		goto threaded_dual_second_done;

second_D8: /* RET C */
		if(WGB_CPU_REG.f.f_bits.c)
		{
        WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
			inst_cycles += 12;
		}

		goto threaded_dual_second_done;

second_D9: /* RETI */
	{
    WGB_CPU_REG.pc.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		gb->gb_ime = true;
	}
	goto threaded_dual_second_done;

second_DA: /* JP C, imm */
		if(WGB_CPU_REG.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT
        WGB_CPU_REG.pc.reg = __gb_read16(gb, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.reg += 2; // advance PC past the immediate word
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

		goto threaded_dual_second_done;

second_DC: /* CALL C, imm */
		if(WGB_CPU_REG.f.f_bits.c)
		{
#if WALNUT_GB_16_BIT
        uint16_t target = __gb_read16(gb, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.reg += 2; // advance PC past the immediate word
        // push current PC onto stack
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.reg = target;
#else
        uint8_t c = __gb_fetch8(gb);
        uint8_t p = __gb_fetch8(gb);
        __gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
        WGB_CPU_REG.pc.bytes.c = c;
        WGB_CPU_REG.pc.bytes.p = p;
#endif
			inst_cycles += 12;
		}
		else
			WGB_CPU_REG.pc.reg += 2;

		goto threaded_dual_second_done;

second_DE: /* SBC A, imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_SBC_R8(val, WGB_CPU_REG.f.f_bits.c);
		goto threaded_dual_second_done;
	}

second_DF: /* RST 0x0018 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0018;
		goto threaded_dual_second_done;

second_E0: /* LD (0xFF00+imm), A */
	{
		const uint8_t io = __gb_fetch8(gb);
		if(WGB_LIKELY(io >= 0x80))
		{
			gb->hram_io[io] = WGB_CPU_REG.a;
		}
		else
			__gb_write(gb, 0xFF00 | io, WGB_CPU_REG.a);
		goto threaded_dual_second_done;
	}

second_E1: /* POP HL */
    WGB_CPU_REG.hl.reg = __gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		goto threaded_dual_second_done;

second_E2: /* LD (C), A */
		if(WGB_LIKELY(WGB_CPU_REG.bc.bytes.c >= 0x80))
		{
			gb->hram_io[WGB_CPU_REG.bc.bytes.c] = WGB_CPU_REG.a;
		}
		else
			__gb_write(gb, 0xFF00 | WGB_CPU_REG.bc.bytes.c, WGB_CPU_REG.a);
		goto threaded_dual_second_done;

second_E5: /* PUSH HL */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.hl.reg);
		goto threaded_dual_second_done;

second_E6: /* AND imm */
	{
		uint8_t temp = __gb_fetch8(gb);
		WGB_INSTR_AND_R8(temp);
		goto threaded_dual_second_done;
	}

second_E7: /* RST 0x0020 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0020;
		goto threaded_dual_second_done;

second_E8: /* ADD SP, imm */
	{
		int8_t offset = (int8_t) __gb_fetch8(gb);
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = ((WGB_CPU_REG.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF);
		WGB_CPU_REG.sp.reg += offset;
		goto threaded_dual_second_done;
	}

second_E9: /* JP (HL) */
		WGB_CPU_REG.pc.reg = WGB_CPU_REG.hl.reg;
		goto threaded_dual_second_done;

second_EA: /* LD (imm), A */
	{
#if WALNUT_GB_16_BIT
    uint16_t addr = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2; // advance past the immediate
#else
    uint8_t l = __gb_fetch8(gb);
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
		__gb_write(gb, addr, WGB_CPU_REG.a);
		goto threaded_dual_second_done;
	}

second_EE: /* XOR imm */
		WGB_INSTR_XOR_R8(__gb_fetch8(gb));
		goto threaded_dual_second_done;

second_EF: /* RST 0x0028 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0028;
		goto threaded_dual_second_done;

second_F0: /* LD A, (0xFF00+imm) */
	{
		const uint16_t stat_read_pc = (uint16_t)(WGB_CPU_REG.pc.reg - 1);
		const uint8_t io = __gb_fetch8(gb);
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(io == IO_STAT)
			WGB_CPU_REG.a = __gb_fast_stat_poll_read(gb,
				stat_read_pc, WGB_CPU_REG.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, io, &WGB_CPU_REG.a))
				WGB_CPU_REG.a = __gb_read(gb, 0xFF00 | io);
		goto threaded_dual_second_done;
	}

second_F1: /* POP AF */
	{
		const uint16_t temp_16 =
			__gb_stack_pop16(gb, &WGB_CPU_REG.sp.reg);
		uint8_t temp_8 = (uint8_t)temp_16;
		WGB_CPU_REG.f.f_bits.z = (temp_8 >> 7) & 1;
		WGB_CPU_REG.f.f_bits.n = (temp_8 >> 6) & 1;
		WGB_CPU_REG.f.f_bits.h = (temp_8 >> 5) & 1;
		WGB_CPU_REG.f.f_bits.c = (temp_8 >> 4) & 1;
		WGB_CPU_REG.a = (uint8_t)(temp_16 >> 8);
		goto threaded_dual_second_done;
	}

second_F2: /* LD A, (C) */
	{
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE
		if(WGB_CPU_REG.bc.bytes.c == IO_STAT)
			WGB_CPU_REG.a = __gb_fast_stat_poll_c_read(gb,
				(uint16_t)(WGB_CPU_REG.pc.reg - 1), WGB_CPU_REG.pc.reg);
		else
#endif
			if(!__gb_fast_io_read_noapu(gb, WGB_CPU_REG.bc.bytes.c, &WGB_CPU_REG.a))
				WGB_CPU_REG.a = __gb_read(gb, 0xFF00 | WGB_CPU_REG.bc.bytes.c);
		goto threaded_dual_second_done;
	}

second_F3: /* DI */
		gb->gb_ime = false;
		goto threaded_dual_second_done;

second_F5: /* PUSH AF */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg,
			((uint16_t)WGB_CPU_REG.a << 8) |
			WGB_CPU_REG.f.f_bits.z << 7 | WGB_CPU_REG.f.f_bits.n << 6 |
			WGB_CPU_REG.f.f_bits.h << 5 | WGB_CPU_REG.f.f_bits.c << 4);
		goto threaded_dual_second_done;

second_F6: /* OR imm */
		WGB_INSTR_OR_R8(__gb_fetch8(gb));
		goto threaded_dual_second_done;

second_F7: /* PUSH AF */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0030;
		goto threaded_dual_second_done;

second_F8: /* LD HL, SP+/-imm */
	{
		/* Taken from SameBoy, which is released under MIT Licence. */
		int8_t offset = (int8_t) __gb_fetch8(gb);
		WGB_CPU_REG.hl.reg = WGB_CPU_REG.sp.reg + offset;
		WGB_CPU_REG.f.reg = 0;
		WGB_CPU_REG.f.f_bits.h = ((WGB_CPU_REG.sp.reg & 0xF) + (offset & 0xF) > 0xF) ? 1 : 0;
		WGB_CPU_REG.f.f_bits.c = ((WGB_CPU_REG.sp.reg & 0xFF) + (offset & 0xFF) > 0xFF) ? 1 : 0;
		goto threaded_dual_second_done;
	}

second_F9: /* LD SP, HL */
		WGB_CPU_REG.sp.reg = WGB_CPU_REG.hl.reg;
		goto threaded_dual_second_done;

second_FA: /* LD A, (imm) */
	{
		const uint16_t stat_read_pc = (uint16_t)(WGB_CPU_REG.pc.reg - 1);
#if WALNUT_GB_16_BIT
    uint16_t addr = __gb_read16(gb, WGB_CPU_REG.pc.reg);
    WGB_CPU_REG.pc.reg += 2; // advance past the immediate
#else
    uint8_t l = __gb_fetch8(gb);
    uint8_t h = __gb_fetch8(gb);
    uint16_t addr = WALNUT_GB_U8_TO_U16(h, l);
#endif
#if WALNUT_GB_FAST_STAT_POLL_ACTIVE && WALNUT_GB_FAST_STAT_POLL_DIRECT_READS
		if(addr == (uint16_t)(IO_ADDR | IO_STAT))
			WGB_CPU_REG.a = __gb_fast_stat_poll_read(gb,
				stat_read_pc, WGB_CPU_REG.pc.reg);
		else
#endif
			if(!__gb_fast_abs_read_hot(gb, addr, &WGB_CPU_REG.a))
				WGB_CPU_REG.a = __gb_read(gb, addr);
		goto threaded_dual_second_done;
	}

second_FB: /* EI */
		gb->gb_ime = true;
		goto threaded_dual_second_done;

second_FE: /* CP imm */
	{
		uint8_t val = __gb_fetch8(gb);
		WGB_INSTR_CP_R8(val);
		goto threaded_dual_second_done;
	}

second_FF: /* RST 0x0038 */
		__gb_stack_push16(gb, &WGB_CPU_REG.sp.reg, WGB_CPU_REG.pc.reg);
		WGB_CPU_REG.pc.reg = 0x0038;
		goto threaded_dual_second_done;

second_invalid:
		/* Return address where invalid opcode that was read. */
		(gb->gb_error)(gb, GB_INVALID_OPCODE, WGB_CPU_REG.pc.reg - 1);
		/* The front-end error handler returns; leave through the normal
		 * exit so runner-local state (lazy post pointer) is torn down. */
		goto threaded_dual_exit;
