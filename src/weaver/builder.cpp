#include "builder.h"

#include <filesystem>

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>
#include <common/message.h>

#include <sch/Subckt.h>
#include <sch/Tapeout.h>
#include <sch/Placer.h>
#include <phy/Script.h>

#include <interpret_chp/export_dot.h>
#include <interpret_flow/export_dot.h>
#include <interpret_flow/export_verilog.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>
#include <interpret_prs/export.h>
#include <interpret_sch/export.h>

#include "../pass/link.h"
#include "../pass/chp_to_flow.h"
#include "../pass/flow_to_verilog.h"
#include "../pass/hse_to_prs.h"
#include "../pass/prs_to_spi.h"
#include "../pass/spi_to_gds.h"

#include "../format/dot.h"

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
		for (id.mod = 0; id.mod < (int)prgm.mods.size(); id.mod++) {
			for (id.index = 0; id.index < (int)prgm.mods[id.mod].terms.size(); id.index++) {
				auto pos = find(todo.begin(), todo.end(), id);
				if (pos != todo.end()) {
					todo.erase(pos);
				}
				todo.push_back(id);
			}
		}
	} else if (not id.hasTerm()) {
		if (id.mod >= (int)prgm.mods.size()) {
			printf("error: module not defined\n");
			return;
		}
		for (id.index = 0; id.index < (int)prgm.modAt(id).terms.size(); id.index++) {
			auto pos = find(todo.begin(), todo.end(), id);
			if (pos != todo.end()) {
				todo.erase(pos);
			}
			todo.push_back(id);
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

		auto pos = find(todo.begin(), todo.end(), id);
		if (pos != todo.end()) {
			todo.erase(pos);
		}
		todo.push_back(id);
	}
}

void Build::build(weaver::Program &prgm) {
	while (not todo.empty()) {
		weaver::TermId id = todo.back();
		todo.pop_back();

		if (prgm.termAt(id).variants.empty()) {
			error("", "term '" + prgm.termAt(id).decl.name + "' had no variants", __FILE__, __LINE__);
			continue;
		}

		if (id.var < 0) {
			id.var = prgm.termAt(id).variants.size()-1;
		}

		std::string dialect = prgm.varAt(id).meta.dialect;
		if (not prgm.varAt(id).meta.has("wv.link")) {
			if (not link(*this, prgm, id)) {
				printf("error: unable to link\n");
			}
			continue;
		}

		if (dialect == "func") {
			if (not flatten(*this, prgm, id)) {
				if (testDecompose) {
					if (not prgm.varAt(id).meta.has("func.decompose")) {
						if (not decompose(*this, prgm, id)) {
							// TODO(edward.bingham) or convert to HSE
							printf("error: unable to flatten or decompose chp\n");
						}
						continue;
					}
				} else {
					printf("error: unable to flatten chp\n");
				}
			}

			if (not chpToFlow(*this, prgm, id)) {
				printf("error: unable to bind flat chp to flow\n");
			}
		} else if (dialect == "flow" and (timing == TIMING_CLOCKED or timing == TIMING_MIXED)) {
			if (not flowToVerilog(*this, prgm, id)) {
				printf("error: unable to synthesize clocked module\n");
			}
		} else if (dialect == "proto" and (timing == TIMING_QDI or timing == TIMING_MIXED)) {
			do {
				if (not elaborate(*this, prgm, id)) {
					printf("error: unable to elaborate stated space\n");
					break;
				}

				if (not conflicts(*this, prgm, id)) {
					printf("error: unable to determine state conflicts\n");
					break;
				}
			} while (encode(*this, prgm, id));

			if (not hseToPrs(*this, prgm, id)) {
				printf("error: unable to generate production rules\n");
			}
		} else if (dialect == "circ") {
			if (not bubble(*this, prgm, id)) {
				printf("warning: unable to bubble reshuffle process\n");
			}

			if (not keepers(*this, prgm, id)) {
				printf("warning: unable to add keepers\n");
			}

			if (not sizing(*this, prgm, id)) {
				printf("error: unable to size prs\n");
			}

			if (not prsToSpi(*this, prgm, id)) {
				printf("error: unable to generate netlist\n");
			}
		} else if (dialect == "spice") {
			if (not prgm.varAt(id).meta.has("spi.mapped")
				and not prgm.varAt(id).meta.has("spi.cell")) {
				if (not noCells and not mapCells(*this, prgm, id)) {
					printf("err: unable to break subckt into cells\n");
				}
				continue;
			}

			if (not spiToGds(*this, prgm, id)) {
				printf("err: unable to build layout\n");
			}
		}
	}
}

