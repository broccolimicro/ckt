#include "builder.h"

#include <filesystem>

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>

#include <chp/synthesize.h>
#include <flow/synthesize.h>

#include <hse/elaborator.h>
#include <hse/encoder.h>
#include <hse/synthesize.h>
#include <prs/bubble.h>
#include <prs/synthesize.h>
#include <sch/Netlist.h>
#include <sch/Tapeout.h>
#include <sch/Placer.h>
#include <phy/Script.h>

#include <interpret_chp/export_dot.h>
#include <interpret_flow/export_dot.h>
#include <interpret_flow/export_verilog.h>
#include <interpret_hse/export_cli.h>
#include <interpret_hse/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>
#include <interpret_prs/export.h>
#include <interpret_sch/export.h>

#include "../pass/chp_to_flow.h"
#include "../pass/flow_to_verilog.h"

#include "../format/cell.h"
#include "../format/dot.h"

#define MAX_PROCESS_SIZE 256

Build::Build(weaver::Project &proj) : proj(proj) {
	logic = LOGIC_CMOS;
	timing = TIMING_MIXED;
	stage = -1;

	doPreprocess = false;
	doPostprocess = false;

	testDecompose = false;

	noCells = false;
	noGhosts = false;

	progress = false;
	debug = false;
	format_expressions_as_html_table = false;
	
	targets.resize(ROUTE+1, false);
}

Build::~Build() {
}

void Build::set(int target) {
	stage = stage < target ? target : stage;
	targets[target] = true;
}

bool Build::get(int target) const {
	return stage < 0 or stage >= target;
}

void Build::inclAll() {
	targets = vector<bool>(ROUTE+1, true);
}

void Build::incl(int target) {
	targets[target] = true;
}

void Build::excl(int target) {
	targets[target] = false;
}

bool Build::has(int target) const {
	return targets[target];
}

void Build::push(weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasMod()) {
		for (int i = 0; i < (int)prgm.mods.size(); i++) {
			for (int j = 0; j < (int)prgm.mods[i].terms.size(); j++) {
				todo.push_back(weaver::TermId(i, j));
			}
		}
	} else if (not id.hasTerm()) {
		if (id.mod >= (int)prgm.mods.size()) {
			printf("error: module not defined\n");
			return;
		}
		for (int j = 0; j < (int)prgm.modAt(id).terms.size(); j++) {
			todo.push_back(weaver::TermId(id.mod, j));
		}
	} else {
		if (id.mod >= (int)prgm.mods.size()) {
			printf("error: module not defined\n");
			return;
		}
		if (id.index >= (int)prgm.modAt(id).terms.size()) {
			printf("error: term not defined in module\n");
			return;
		}
		if (id.var >= (int)prgm.termAt(id).variants.size()) {
			printf("error: variant not defined in term\n");
			return;
		}

		todo.push_back(id);
	}
}

void Build::build(weaver::Program &prgm) {
	while (not todo.empty()) {
		weaver::TermId id = todo.back();
		todo.pop_back();

		if (prgm.termAt(id).variants.empty()) {
			printf("error: term had no variants\n");
			continue;
		}

		if (id.var < 0) {
			id.var = prgm.termAt(id).variants.size()-1;
		}

		std::string dialect = prgm.varAt(id).meta.dialect();
		if (dialect == "func") {
			if (flatten(*this, prgm, id)) {
				if (not chpToFlow(*this, prgm, id, todo)) {
					printf("error: unable to bind flat chp to flow\n");
				}
			} else if (not decompose(*this, prgm, id, todo)) {
				printf("error: unable to decompose chp\n");
			}
		} else if (dialect == "flow") {
			if (not flowToVerilog(*this, prgm, id, todo)) {
				printf("error: unable to synthesize clocked module\n");
			}
		}
	}
}

