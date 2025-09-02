#include "builder.h"

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

#include <interpret_flow/export.h>
#include <interpret_hse/export_cli.h>
#include <interpret_hse/export.h>
#include <interpret_prs/export.h>
#include <interpret_sch/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

#include <filesystem>

#include "../format/cell.h"

Build::Build(weaver::Project &proj) : proj(proj) {
	logic = LOGIC_CMOS;
	timing = TIMING_MIXED;
	stage = -1;

	doPreprocess = false;
	doPostprocess = false;

	noCells = false;
	noGhosts = false;
	
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
}

void Build::build(weaver::Program &prgm) {
	for (int i = 0; i < (int)prgm.mods.size(); i++) {
		for (int j = 0; j < (int)prgm.mods[i].terms.size(); j++) {
			if (prgm.mods[i].terms[j].kind < 0) {
				printf("internal:%s:%d: dialect not defined for term '%s'\n", __FILE__, __LINE__, prgm.mods[i].terms[j].decl.name.c_str());
				continue;
			}
			string dialectName = prgm.mods[i].terms[j].dialect().name;
			if (dialectName == "func") {
				chpToFlow(prgm, i, j);
			} else if (dialectName == "flow") {
				flowToVerilog(prgm, i, j);
			} else if (dialectName == "proto") {
				hseToPrs(prgm, i, j);
			} else if (dialectName == "circ") {
				prsToSpi(prgm, i, j);
			} else if (dialectName == "spice") {
				spiToGds(prgm, i, j);
			}
		}
	}
}

bool Build::chpToFlow(weaver::Program &prgm, int modIdx, int termIdx) const {
	string debugDir = proj.rootDir / proj.BUILD / "dbg";

	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "func") {
		printf("error: dialect '%s' not supported for translation from chp to flow.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int flowKind = weaver::Term::getDialect("flow");
	int flowIdx = prgm.getModule(prgm.mods[modIdx].name + ">>flow");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: function must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the flow module
	string name = decl.name;
	// TODO(edward.bingham) merge weaver type system and flow types?
	vector<weaver::Instance> args = decl.args;

	// Do the synthesis
	chp::graph &g = prgm.mods[modIdx].terms[termIdx].as<chp::graph>();
	g.post_process();	

	//string graph_render_filename = "_" + prefix + "_" + g.name + ".png";
	//gvdot::render(graph_render_filename, chp::export_graph(g, true).to_string());

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to flow
	}

	int dstIdx = prgm.mods[flowIdx].createTerm(weaver::Term::procOf(flowKind, name, args));
	prgm.mods[flowIdx].terms[dstIdx].def = chp::synthesizeFuncFromCHP(g);
	return true;
}

bool Build::flowToVerilog(weaver::Program &prgm, int modIdx, int termIdx) const {
	string debugDir = proj.rootDir / proj.BUILD / "dbg";

	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "flow") {
		printf("error: dialect '%s' not supported for translation from flow to val-rdy.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int verilogKind = weaver::Term::getDialect("verilog");
	int verilogIdx = prgm.getModule(prgm.mods[modIdx].name + ">>verilog");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: flow must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the verilog module
	string name = decl.name;
	// TODO(edward.bingham) merge weaver type system and verilog types?
	vector<weaver::Instance> args = decl.args;

	// Do the synthesis
	flow::Func &fn = prgm.mods[modIdx].terms[termIdx].as<flow::Func>();

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to verilog
	}

	int dstIdx = prgm.mods[verilogIdx].createTerm(weaver::Term::procOf(verilogKind, name, args));
	prgm.mods[verilogIdx].terms[dstIdx].def = flow::synthesizeModuleFromFunc(fn);
	return true;
}

bool Build::hseToPrs(weaver::Program &prgm, int modIdx, int termIdx) const {
	string debugDir = proj.rootDir / proj.BUILD / "dbg";

	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "proto") {
		printf("error: dialect '%s' not supported for translation from hse to prs.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int cktKind = weaver::Term::getDialect("circ");
	int cktIdx = prgm.getModule(prgm.mods[modIdx].name + ">>circ");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: protocol must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = decl.name;
	vector<weaver::Instance> args = decl.args;

	hse::graph &hg = prgm.mods[modIdx].terms[termIdx].as<hse::graph>();
	hg.name = decl.name;
	hg.post_process(true);
	hg.check_variables();

	if (get(Build::ELAB)) {
		if (progress) printf("Elaborate state space:\n");
		hse::elaborate(hg, stage >= Build::ENCODE or not noGhosts, true, progress);
		if (progress) printf("done\n\n");

		if (has(Build::ELAB)) {
			std::filesystem::create_directories(debugDir);
			string suffix = stage == Build::ELAB ? "" : "_predicate";
			string filename = debugDir+"/"+prgm.mods[modIdx].terms[termIdx].decl.name+suffix+".astg";
			hse::export_astg(filename, hg);
		}

		if (not is_clean()) {
			return false;
		}
	}

	bool inverting = false;
	if (logic == Build::LOGIC_CMOS) {
		inverting = true;
	}

	hse::encoder enc;
	enc.base = &hg;
	if (get(Build::CONFLICTS)) {
		if (progress) printf("Identify state conflicts:\n");
		enc.check(!inverting, progress);
		if (progress) printf("done\n\n");

		if (has(Build::CONFLICTS)) {
			print_conflicts(enc);
		}
	}

	if (get(Build::ENCODE)) {
		if (progress) printf("Insert state variables:\n");
		if (not enc.insert_state_variables(20, !inverting, progress, true)) {
			return false;
		}
		if (progress) printf("done\n\n");

		if (has(Build::ENCODE)) {
			string suffix = stage == Build::ENCODE ? "" : "_complete";
			hse::export_astg(debugDir+"/"+hg.name+suffix+".astg", hg);
		}

		if (enc.conflicts.size() > 0) {
			// state variable insertion failed
			print_conflicts(enc);
			return false;
		}
	}

	if (get(Build::RULES)) {
		if (progress) printf("Synthesize production rules:\n");
		prs::production_rule_set pr;
		hse::synthesize_rules(&pr, &hg, !inverting, progress);
		if (progress) printf("done\n\n");

		int dstIdx = prgm.mods[cktIdx].createTerm(weaver::Term::procOf(cktKind, name, args));
		prgm.mods[cktIdx].terms[dstIdx].def = pr;
	}
	return true;
}

bool Build::prsToSpi(weaver::Program &prgm, int modIdx, int termIdx) {
	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "circ") {
		printf("error: dialect '%s' not supported for translation from prs to spi.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int spiKind = weaver::Term::getDialect("spice");
	int spiIdx = prgm.getModule(prgm.mods[modIdx].name + ">>spice");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: circuit must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = decl.name;
	vector<weaver::Instance> args = decl.args;

	prs::production_rule_set &pr = prgm.mods[modIdx].terms[termIdx].as<prs::production_rule_set>();

	bool inverting = false;
	if (logic == Build::LOGIC_CMOS) {
		inverting = true;
	}

	if (get(Build::BUBBLE) and inverting and not pr.cmos_implementable()) {
		if (progress) {
			printf("Bubble reshuffle production rules:\n");
			printf("  %s...", pr.name.c_str());
			fflush(stdout);
		}
		prs::bubble bub;
		bub.load_prs(pr);

		int step = 0;
		//if (has(Build::BUBBLE) and debug) {
		//	gvdot::render(pr.name+"_bubble0.png", export_bubble(bub, pr).to_string());
		//}
		for (auto i = bub.net.begin(); i != bub.net.end(); i++) {
			auto result = bub.step(i);
			//if (has(Build::BUBBLE) and debug and result.second) {
			//	gvdot::render(pr.name+"_bubble" + to_string(++step) + ".png", export_bubble(bub, pr).to_string());
			//}
		}
		auto result = bub.complete();
		//if (has(Build::BUBBLE) and debug and result) {
		//	gvdot::render(pr.name+"_bubble" + to_string(++step) + ".png", export_bubble(bub, pr).to_string());
		//}

		bub.save_prs(&pr);
		if (progress) {
			printf("[%sDONE%s]\n", KGRN, KNRM);
			printf("done\n\n");
		}
	}

	if (get(Build::KEEPERS)) {
		if (logic == Build::LOGIC_CMOS or logic == Build::LOGIC_RAW) {
			if (progress) printf("Insert keepers:\n");
			pr.add_keepers(true, false, 1, progress);
			if (progress) printf("done\n\n");
		}
	}

	if (get(Build::SIZE)) {
		if (progress) printf("Size production rules:\n");
		pr.size_devices(0.1, progress);
		if (progress) printf("done\n\n");
	}
	
	if (get(Build::NETS)) {
		if (not proj.tech.isLoaded() and not phy::loadTech(proj.tech)) {
			cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
			return false;
		}

		sch::Netlist net;
		if (progress) printf("Build netlist:\n");
		net.subckts.push_back(prs::build_netlist(proj.tech, pr, progress));
		if (progress) printf("done\n\n");
		if (debug) {
			net.subckts.back().print();
		}

		int dstIdx = prgm.mods[spiIdx].createTerm(weaver::Term::procOf(spiKind, name, args));
		prgm.mods[spiIdx].terms[dstIdx].def = net;
	}
	return true;
}

bool Build::spiToGds(weaver::Program &prgm, int modIdx, int termIdx) {
	string debugDir = proj.rootDir / proj.BUILD / "dbg";

	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "spice") {
		printf("error: dialect '%s' not supported for translation from spice to gds.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int gdsKind = weaver::Term::getDialect("layout");
	int gdsIdx = prgm.getModule(prgm.mods[modIdx].name + ">>layout");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: spice must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = decl.name;
	vector<weaver::Instance> args = decl.args;

	sch::Netlist &net = prgm.mods[modIdx].terms[termIdx].as<sch::Netlist>();

	if (noCells) {
		for (int i = 0; i < (int)net.subckts.size(); i++) {
			net.subckts[i].isCell = true;
		}
	}

	if (not proj.tech.isLoaded() and not phy::loadTech(proj.tech)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return false;
	}

	Timer cellsTmr;
	if (get(Build::MAP)) {
		if (progress) printf("Break subckts into cells:\n");
		net.mapCells(proj.tech, progress);
		if (progress) printf("done\t%gs\n\n", cellsTmr.since());
	}

	phy::Library lib(proj.tech);
	map<int, gdstk::Cell*> cells;
	if (get(Build::CELLS)) {
		cell::update_library(lib, net, nullptr, &cells, progress, debug);
	}

	if (get(Build::PLACE)) {
		doPlacement(lib, net, nullptr, &cells, progress);
	}

	int dstIdx = prgm.mods[gdsIdx].createTerm(weaver::Term::procOf(gdsKind, name, args));
	prgm.mods[gdsIdx].terms[dstIdx].def = lib;
	return true;
}

