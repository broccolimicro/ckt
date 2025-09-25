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

#include "../format/cell.h"
#include "../format/dot.h"

Build::Build(weaver::Project &proj) : proj(proj) {
	logic = LOGIC_CMOS;
	timing = TIMING_MIXED;
	stage = -1;

	doPreprocess = false;
	doPostprocess = false;

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

void doPlacement(phy::Library &lib, sch::Netlist &lst, gdstk::GdsWriter *stream=nullptr, map<int, gdstk::Cell*> *cells=nullptr, bool progress=false, bool debug=false) {
	if (progress) {
		printf("Placing Cells:\n");
	}

	if (lib.macros.size() < lst.subckts.size()) {
		lib.macros.resize(lst.subckts.size(), Layout(*lib.tech));
	}

	sch::Placer placer(lib, lst, 0, 0, progress, debug);

	Timer total;
	for (int i = 0; i < (int)lst.subckts.size(); i++) {
		if (not lst.subckts[i].isCell) {
			if (progress) {
				printf("  %s...", lst.subckts[i].name.c_str());
				fflush(stdout);
			}
			Timer tmr;
			lib.macros[i].name = lst.subckts[i].name;
			placer.place(i);
			if (progress) {
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
			lst.mapToLayout(i, lib.macros[i]);
		}
	}

	if (progress) {
		printf("done\t%gs\n\n", total.since());
	}
}

void Build::build(weaver::Program &prgm, weaver::TermId term) {
	if (term.mod < 0) {
		for (term.mod = 0; term.mod < (int)prgm.mods.size(); term.mod++) {
			build(prgm, term);
		}
	} else if (term.index < 0) {
		for (term.index = 0; term.index < (int)prgm.mods[term.mod].terms.size(); term.index++) {
			build(prgm, term);
		}
	} else {
		if (prgm.mods[term.mod].terms[term.index].kind < 0) {
			printf("internal:%s:%d: dialect not defined for term '%s'\n", __FILE__, __LINE__, prgm.mods[term.mod].terms[term.index].decl.name.c_str());
			return;
		}
		string dialectName = prgm.mods[term.mod].terms[term.index].dialect().name;
		if (dialectName == "func") {
			chpToFlow(prgm, term.mod, term.index);
		} else if (dialectName == "flow") {
			flowToVerilog(prgm, term.mod, term.index);
		} else if (dialectName == "proto") {
			hseToPrs(prgm, term.mod, term.index);
		} else if (dialectName == "circ") {
			prsToSpi(prgm, term.mod, term.index);
		} else if (dialectName == "spice") {
			spiToGds(prgm, term.mod, term.index);
		}
	}
}

bool Build::chpToFlow(weaver::Program &prgm, int modIdx, int termIdx) const {
	std::filesystem::path debugDirPath = proj.rootDir / proj.BUILD / "dbg";
	string debugDir = debugDirPath.string();

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
	g.post_process(true);	
	if (this->debug) {
		string prefix = ""; //"_" + this->proj.modName + "_";
		string chp_filename = (debugDirPath / (prefix + g.name + "_chp.png")).string();
		string chp_dot = chp::export_graph(g, true).to_string();
		gvdot::render(chp_filename, chp_dot);
	}

	g.flatten(this->debug);

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to flow
		chp::variable var(i->name);
		g.vars.push_back(var);
		//TODO: can chp::synthesizeFuncFromCHP() always assume its vars are pre-populated?
	}

	int dstIdx = prgm.mods[flowIdx].createTerm(weaver::Term::procOf(flowKind, name, args));
	const flow::Func &f = chp::synthesizeFuncFromCHP(g);
	prgm.mods[flowIdx].terms[dstIdx].def = f;

	if (this->debug) {
		string prefix = ""; //"_" + this->proj.modName + "_";
		string flatchp_filename = (debugDirPath / (prefix + g.name + "_flatchp.png")).string();
		string flatchp_dot = chp::export_graph(g, true).to_string();
		gvdot::render(flatchp_filename, flatchp_dot);

		string flow_filename = (debugDirPath / (prefix + g.name + "_flow.dot")).string();
		string flow_dot = flow::export_func(f, this->format_expressions_as_html_table).to_string();
		//gvdot::render(flow_filename, flow_dot);
		//TODO: a well-structured flow::export_func in interpret_flow/export_dot,h will play nice with gvdot::render for png export

		std::ofstream export_file(flow_filename);
		if (!export_file) {
				std::cerr << "ERROR: Failed to open file for dot export: "
					<< flow_filename << std::endl;
					//<< "ERROR: Try again from dir: <project_dir>/lib/flow" << std::endl;

				//TODO: we want soft failure, but this doesn't break or prevent file writing
				return false;

		}  else {
			export_file << flow_dot;
		}
	}

	return true;
}

bool Build::flowToVerilog(weaver::Program &prgm, int modIdx, int termIdx) const {
	std::filesystem::path debugDirPath = proj.rootDir / proj.BUILD / "dbg";
	string debugDir = debugDirPath.string();

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
	clocked::Module mod = flow::synthesizeModuleFromFunc(fn);
	parse_verilog::module_def mod_v = flow::export_module(mod);
	string verilog = mod_v.to_string();

	prgm.mods[verilogIdx].terms[dstIdx].def = mod;
	//TODO: Why stop at clocked::Module? This is flowToVerilog!
	//prgm.mods[verilogIdx].terms[dstIdx].def = mod_v; ??
	//prgm.mods[verilogIdx].terms[dstIdx].def = verilog; ??


	if (this->debug) {
		string prefix = ""; //"_" + this->proj.modName + "_";
		string verilog_filename = (debugDirPath / (prefix + fn.name + ".v")).string();

		std::ofstream export_file(verilog_filename);
		if (!export_file) {
				std::cerr << "ERROR: Failed to open file for verilog export: "
					<< verilog_filename << std::endl;
					//<< "ERROR: Try again from dir: <project_dir>/lib/flow" << std::endl;

				//TODO: we want soft failure, but this doesn't break or prevent file writing
				return false;

		}  else {
			export_file << verilog;
		}
	}

	return true;
}

bool Build::hseToPrs(weaver::Program &prgm, int modIdx, int termIdx) const {
	std::filesystem::path debugDirPath = proj.rootDir / proj.BUILD / "dbg";
	string debugDir = debugDirPath.string();

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
			string filename = debugDir+"/"+prgm.mods[modIdx].terms[termIdx].decl.name+suffix+".astgw";
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
		if (not enc.insert_state_variables(20, !inverting, progress, debug)) {
			return false;
		}
		if (progress) printf("done\n\n");

		if (has(Build::ENCODE)) {
			string suffix = stage == Build::ENCODE ? "" : "_complete";
			hse::export_astg(debugDir+"/"+hg.name+suffix+".astgw", hg);
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

		//int step = 0;
		//if (has(Build::BUBBLE) and debug) {
		//	gvdot::render(pr.name+"_bubble0.png", export_bubble(bub, pr).to_string());
		//}
		for (auto i = bub.net.begin(); i != bub.net.end(); i++) {
			bub.step(i);
			//auto result = bub.step(i);
			//if (has(Build::BUBBLE) and debug and result.second) {
			//	gvdot::render(pr.name+"_bubble" + to_string(++step) + ".png", export_bubble(bub, pr).to_string());
			//}
		}
		bub.complete();
		//auto result = bub.complete();
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
	std::filesystem::path debugDirPath = proj.rootDir / proj.BUILD / "dbg";
	string debugDir = debugDirPath.string();

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
		doPlacement(lib, net, nullptr, &cells, progress, debug);
	}

	int dstIdx = prgm.mods[gdsIdx].createTerm(weaver::Term::procOf(gdsKind, name, args));
	prgm.mods[gdsIdx].terms[dstIdx].def = lib;
	return true;
}

