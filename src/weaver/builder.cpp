#include "builder.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include <chp/graph.h>
//#include <chp/simulator.h>
//#include <parse_chp/composition.h>
#include <interpret_chp/import.h>
#include <interpret_chp/export.h>

#include <hse/graph.h>
#include <hse/simulator.h>
#include <hse/encoder.h>
#include <hse/elaborator.h>
#include <hse/synthesize.h>
#include <interpret_hse/import.h>
#include <interpret_hse/export.h>
#include <interpret_hse/export_cli.h>

#include <prs/production_rule.h>
#include <prs/bubble.h>
#include <prs/synthesize.h>
#include <interpret_prs/import.h>
#include <interpret_prs/export.h>

#include <sch/Netlist.h>
#include <sch/Tapeout.h>
#include <sch/Placer.h>
#include <interpret_sch/import.h>
#include <interpret_sch/export.h>

#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Layout.h>
#include <phy/Library.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

//#include <parse_expression/expression.h>
//#include <parse_expression/assignment.h>
//#include <parse_expression/composition.h>
#include <interpret_boolean/export.h>
#include <interpret_boolean/import.h>
#include <interpret_arithmetic/export.h>
#include <interpret_arithmetic/import.h>

#include <filesystem>

#include "symbol.h"
//#include "cell.h"

void Build::set(int target) {
	stage = stage < target ? target : stage;
	targets[target] = true;
}

bool Build::get(int target) {
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

bool Build::has(int target) {
	return targets[target];
}

/*bool canonicalize_hse(const Build &builder, weaver::Module &tbl, Function &func) {
	func.hg->post_process(true);
	func.hg->check_variables();

	if (builder.doPostprocess) {
		export_astg(func.hg->name+"_post.astg", *func.hg);
	}
	return true;
}

bool elaborate_hse(const Build &builder, weaver::Module &tbl, Function &func) {
	if (builder.progress) printf("Elaborate state space:\n");
	hse::elaborate(*func.hg, builder.stage >= Build::ENCODE or not builder.noGhosts, true, builder.progress);
	if (builder.progress) printf("done\n\n");
	return true;
}*/

/*bool encode_hse(const Build &builder, weaver::Module &tbl, Function &func) {
	hse::encoder enc;
	enc.base = &func.hg;
	enc.variables = &func.v;

	if (progress) {
		printf("Identify state conflicts:\n");
	}
	enc.check(!inverting, progress);
	if (progress) {
		printf("done\n\n");
	}

	if (builder.has(Build::CONFLICTS)) {
		print_conflicts(enc);
	}

	if (not builder.get(Build::ENCODE)) {
		if (progress) printf("compiled in %gs\n\n", totalTime.since());
		complete();
		return is_clean();
	}

	if (progress) printf("Insert state variables:\n");
	if (not enc.insert_state_variables(20, !inverting, progress, false)) {
		if (progress) printf("compiled in %gs\n\n", totalTime.since());
		complete();
		return 1;
	}
	if (progress) printf("done\n\n");

	if (builder.has(Build::ENCODE)) {
		string suffix = builder.stage == Build::ENCODE ? "" : "_complete";
		export_astg(prefix+suffix+".astg", hg, v);
	}

	if (enc.conflicts.size() > 0) {
		// state variable insertion failed
		print_conflicts(enc);
		if (progress) printf("compiled in %gs\n\n", totalTime.since());
		complete();
		return is_clean();
	}
	return true;
}*/

/*void hse_to_prs(const Build &builder, weaver::Module &tbl, Structure &strct, const Function &func) {
	
}

void doPlacement(phy::Library &lib, sch::Netlist &lst, gdstk::GdsWriter *stream=nullptr, map<int, gdstk::Cell*> *cells=nullptr, bool report_progress=false) {
	if (report_progress) {
		printf("Placing Cells:\n");
	}

	if (lib.macros.size() < lst.subckts.size()) {
		lib.macros.resize(lst.subckts.size(), Layout(*lib.tech));
	}

	sch::Placer placer(lib, lst);

	Timer total;
	for (int i = 0; i < (int)lst.subckts.size(); i++) {
		if (not lst.subckts[i].isCell) {
			if (report_progress) {
				printf("  %s...", lst.subckts[i].name.c_str());
				fflush(stdout);
			}
			Timer tmr;
			lib.macros[i].name = lst.subckts[i].name;
			placer.place(i);
			if (report_progress) {
				int area = 0;
				for (auto j = lst.subckts[i].inst.begin(); j != lst.subckts[i].inst.end(); j++) {
					if (lst.subckts[j->subckt].isCell) {
						area += lib.macros[j->subckt].box.area();
					}
				}
				printf("[%s%d DBUNIT2 AREA%s]\t%gs\n", KGRN, area, KNRM, tmr.since());
			}
			if (stream != nullptr and cells != nullptr) {
				export_layout(*stream, lib, i, *cells);
			}
		}
	}

	if (report_progress) {
		printf("done\t%gs\n\n", total.since());
	}
}*/

