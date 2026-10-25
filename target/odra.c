//
// Created by piotr on 22/10/2026.
//
#include <ir/ir.h>
#include <target/util.h>

#define ODRA_ADDRESS_START 64
#define ODRA_TABLE_BASE 13000000

#define ODRA_TMP0 7
#define ODRA_TMP1 8
#define ODRA_ADDR 9
#define ODRA_JUMP 10

static int odra_label_id;

static int odra_translate_registers(Reg r) {
	static const int cells[] = {1, 2, 3, 4, 5, 6};
	return cells[r];
}

static int odra_translate_address(const int addr) {
	return ODRA_ADDRESS_START + addr;
}

static const char *odra_attach_operands(Value *v) {
	// registers
	if (v->type == REG) {
		return format("%d", odra_translate_registers(v->reg));
	}
	if (v->type == IMM) {
		// direct
		return format("=%d", v->imm);
	}
	error("odra backend: invalid value");
}

static int odra_new_label() {
	return odra_label_id++;
}

// translate addresses to account of the offset
static void odra_emit_effective_address(const char *cell) {
	emit_line("load %s", cell);
	emit_line("add =%d", ODRA_ADDRESS_START);
}

static void odra_emit_comparison_result(Inst *inst) {
	const int is_positive = odra_new_label();
	const int is_negative = odra_new_label();
	const int done = odra_new_label();

	emit_line("load %d", odra_translate_registers(inst->dst.reg));
	emit_line("store %d", ODRA_TMP0);
	emit_line("load %s", odra_attach_operands(&inst->src));
	emit_line("store %d", ODRA_TMP1);

	const Op op = normalize_cond(inst->op, 0);
	if (op == JEQ || op == JNE) {
		emit_line("load %d", ODRA_TMP0);
		emit_line("sub %d", ODRA_TMP1);
		return;
	}

	// If signs differ, result is purely determined by sign of dst:
	// dst >= 0 && src < 0  =>  dst > src (+1)
	// dst < 0  && src >= 0 =>  dst < src (-1)
	const int same_sign = odra_new_label();
	const int dst_neg = odra_new_label();
	emit_line("load %d", ODRA_TMP0);
	emit_line("jgtz C%d", is_positive);
	emit_line("jzero C%d", is_positive);
	emit_line("jump C%d", dst_neg);

	emit_line("C%d: load %d", is_positive, ODRA_TMP1);
	emit_line("jgtz C%d", same_sign);
	emit_line("jzero C%d", same_sign);
	emit_line("load =1");
	emit_line("jump C%d", done);

	emit_line("C%d: load %d", dst_neg, ODRA_TMP1);
	emit_line("jgtz C%d", is_negative);
	emit_line("jzero C%d", is_negative);
	emit_line("jump C%d", same_sign);

	emit_line("C%d: load =-1", is_negative);
	emit_line("jump C%d", done);

	emit_line("C%d: load %d", same_sign, ODRA_TMP0);
	emit_line("sub %d", ODRA_TMP1);
	emit_line("C%d: add =0", done);
}

static void odra_generate_jump(Op op, const char *target) {
	op = normalize_cond(op, 0);
	switch (op) {
		case JEQ:
			emit_line("jzero %s", target);
			break;
		case JNE: {
			const int skip = odra_new_label();
			emit_line("jzero C%d", skip);
			emit_line("jump %s", target);
			emit_line("C%d: load 0", skip);
			break;
		}
		case JLT: {
			const int skip = odra_new_label();
			emit_line("jgtz C%d", skip);
			emit_line("jzero C%d", skip);
			emit_line("jump %s", target);
			emit_line("C%d: load 0", skip);
			break;
		}
		case JLE: {
			const int skip = odra_new_label();
			emit_line("jgtz C%d", skip);
			emit_line("jump %s", target);
			emit_line("C%d: load 0", skip);
			break;
		}
		case JGT:
			emit_line("jgtz %s", target);
			break;
		case JGE: {
			const int skip = odra_new_label();
			emit_line("jgtz %s", target);
			emit_line("jzero %s", target);
			emit_line("jump C%d", skip);
			emit_line("C%d: load 0", skip);
			break;
		}
		case JMP:
			emit_line("jump %s", target);
			break;
		default:
			error("odra backend: invalid comparison");
	}
}

static void odra_emit_boolean_comparison(Inst *inst) {
	const char *true_label = format("C%d", odra_new_label());
	const char *end_label = format("C%d", odra_new_label());

	odra_emit_comparison_result(inst);
	odra_generate_jump(inst->op, true_label);
	emit_line("load =0");
	emit_line("store %d", odra_translate_registers(inst->dst.reg));
	emit_line("jump %s", end_label);
	emit_line("%s: load =1", true_label);
	emit_line("store %d", odra_translate_registers(inst->dst.reg));
	emit_line("%s: load 0", end_label);
}

static void odra_emit_computed_jump(Value *jmp) {
	if (jmp->type != REG) {
		error("odra backend: invalid computed jump");
	}
	emit_line("load %d", odra_translate_registers(jmp->reg));
	emit_line("add =%d", ODRA_TABLE_BASE);
	emit_line("store %d", ODRA_ADDR);
	emit_line("load ^%d", ODRA_ADDR);
	emit_line("store %d", ODRA_JUMP);
	emit_line("jump ^%d", ODRA_JUMP);
}

static void odra_emit_inst(Inst *inst) {
	switch (inst->op) {
		case MOV:
			emit_line("load %s", odra_attach_operands(&inst->src));
			emit_line("store %d", odra_translate_registers(inst->dst.reg));
			break;
		case ADD:
			emit_line("load %d", odra_translate_registers(inst->dst.reg));
			emit_line("add %s", odra_attach_operands(&inst->src));
			emit_line("store %d", odra_translate_registers(inst->dst.reg));
			break;
		case SUB:
			emit_line("load %d", odra_translate_registers(inst->dst.reg));
			emit_line("sub %s", odra_attach_operands(&inst->src));
			emit_line("store %d", odra_translate_registers(inst->dst.reg));
			break;
		case MUL:
			emit_line("load %d", odra_translate_registers(inst->dst.reg));
			emit_line("mult %s", odra_attach_operands(&inst->src));
			emit_line("store %d", odra_translate_registers(inst->dst.reg));
			break;
		case DIV:
			emit_line("load %d", odra_translate_registers(inst->dst.reg));
			emit_line("div %s", odra_attach_operands(&inst->src));
			emit_line("store %d", odra_translate_registers(inst->dst.reg));
			break;
		case MOD:
			emit_line("load %d", odra_translate_registers(inst->dst.reg));
			emit_line("store %d", ODRA_TMP0);
			emit_line("div %s", odra_attach_operands(&inst->src));
			emit_line("mult %s", odra_attach_operands(&inst->src));
			emit_line("store %d", ODRA_TMP1);
			emit_line("load %d", ODRA_TMP0);
			emit_line("sub %d", ODRA_TMP1);
			emit_line("store %d", odra_translate_registers(inst->dst.reg));
			break;
		case LOAD:
			if (inst->src.type == IMM) {
				emit_line("load %d", odra_translate_address(inst->src.imm));
			} else if (inst->src.type == REG) {
				odra_emit_effective_address(format("%d", odra_translate_registers(inst->src.reg)));
				emit_line("store %d", ODRA_ADDR);
				emit_line("load ^%d", ODRA_ADDR);
			} else {
				error("odra backend: invalid value");
			}
			emit_line("store %d", odra_translate_registers(inst->dst.reg));
			break;
		case STORE:
			if (inst->src.type == IMM) {
				emit_line("load %d", odra_translate_registers(inst->dst.reg));
				emit_line("store %d", odra_translate_address(inst->src.imm));
			} else if (inst->src.type == REG) {
				odra_emit_effective_address(format("%d", odra_translate_registers(inst->src.reg)));
				emit_line("store %d", ODRA_ADDR);
				emit_line("load %d", odra_translate_registers(inst->dst.reg));
				emit_line("store ^%d", ODRA_ADDR);
			} else {
				error("odra backend: invalid value");
			}
			break;
		case PUTC:
			emit_line("writec %s", odra_attach_operands(&inst->src));
			break;
		case GETC:
			emit_line("readc %d", odra_translate_registers(inst->dst.reg));
			break;
		case EXIT:
			emit_line("halt");
			break;
		case DUMP:
			// TODO: try to link this to debug
			break;
		case EQ:
		case NE:
		case LT:
		case GT:
		case LE:
		case GE:
			odra_emit_boolean_comparison(inst);
			break;

		case JEQ:
		case JNE:
		case JLT:
		case JGT:
		case JLE:
		case JGE:
			odra_emit_comparison_result(inst);
			if (inst->jmp.type == IMM) {
				odra_generate_jump(inst->op, format("L%d", inst->jmp.imm));
			} else if (inst->jmp.type == REG) {
				const int true_label = odra_new_label();
				odra_generate_jump(inst->op, format("C%d", true_label));
				const int end_label = odra_new_label();
				emit_line("jump C%d", end_label);
				emit_line("C%d: load 0", true_label);
				odra_emit_computed_jump(&inst->jmp);
				emit_line("C%d: load 0", end_label);
			} else {
				error("odra backend: invalid jump");
			}
			break;

		case JMP:
			if (inst->jmp.type == IMM) {
				emit_line("jump L%d", inst->jmp.imm);
			} else if (inst->jmp.type == REG) {
				odra_emit_computed_jump(&inst->jmp);
			} else {
				error("odra backend: invalid jump");
			}
			break;

		default:
			error("odra backend: Instruction not implemented.");
	}
}

// for mass loading of binary data
static void odra_emit_data(const Data *data) {
	int mp = 0;
	for (; data; data = data->next, mp++) {
		if (data->v) {
			emit_line("load =%d", data->v);
			emit_line("store %d", odra_translate_address(mp));
		}
	}
}

static void odra_emit_jump_lookup_table(const Module *module) {
	int last_pc = -1;
	for (const Inst *inst = module->text; inst; inst = inst->next) {
		if (inst->pc == last_pc) {
			continue;
		}
		last_pc = inst->pc;
		emit_line("load =L%d", inst->pc);
		emit_line("store %d", ODRA_TABLE_BASE + inst->pc);
	}
}

void target_odra(Module *module) {
	bool needs_program_counter_to_code_address_lookup_table = false;
	for (const Inst *inst = module->text; inst; inst = inst->next) {
		if ((inst->op == JMP ||
		     inst->op == JEQ || inst->op == JNE ||
		     inst->op == JLT || inst->op == JGT ||
		     inst->op == JLE || inst->op == JGE) &&
		    inst->jmp.type == REG) {
			needs_program_counter_to_code_address_lookup_table = true;
		}
	}

	odra_label_id = 0;
	emit_line("load =8388000"); // sets the IR stack pointer	
	emit_line("store 6");
	odra_emit_data(module->data);

	if (needs_program_counter_to_code_address_lookup_table) {
		odra_emit_jump_lookup_table(module);
	}

	int last_pc = -1;
	for (Inst *inst = module->text; inst; inst = inst->next) {
		if (inst->pc != last_pc) {
			emit_line("L%d: load 0", inst->pc);
			last_pc = inst->pc;
		}
		odra_emit_inst(inst);
	}
}
