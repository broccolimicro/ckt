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
#include <interpret_prs/export.h>
#include <interpret_sch/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

#include <filesystem>

#include "../format/cell.h"

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
			string dialectName = prgm.mods[i].terms[j].dialect().name;
			if (dialectName == "func") {
				chpToFlow(prgm, i, j);
			} else if (dialectName == "__flow__") {
				flowToVerilog(prgm, i, j);
			} else if (dialectName == "proto") {
				hseToPrs(prgm, i, j);
			} else if (dialectName == "ckt") {
				prsToSpi(prgm, i, j);
			} else if (dialectName == "__spice__") {
				spiToGds(prgm, i, j);
			}
		}
	}
}

string Build::getBuildDir(string dialectName) const {
	if (dialectName == "__verilog__") {
		return "rtl";
	} else if (dialectName == "__spice__") {
		return "spi";
	} else if (dialectName == "__layout__") {
		return "gds";
	} else if (dialectName == "ckt") {
		return "ckt";
	}
	return "dbg";
}

void Build::emit(string path, const weaver::Program &prgm) const {
	for (int i = 0; i < (int)prgm.mods.size(); i++) {
		for (int j = 0; j < (int)prgm.mods[i].terms.size(); j++) {
			string dialectName = prgm.mods[i].terms[j].dialect().name;
			string buildDir = path+"/"+getBuildDir(dialectName);
			if (dialectName == "__verilog__") {
				emitVerilog(buildDir, prgm, i, j);
			} else if (dialectName == "ckt") {
				emitPrs(buildDir, prgm, i, j);
			} else if (dialectName == "__spice__") {
				emitSpice(buildDir, prgm, i, j);
			} else if (dialectName == "__layout__") {
				emitGds(buildDir, prgm, i, j);
			}
		}
	}
}

bool Build::chpToFlow(weaver::Program &prgm, int modIdx, int termIdx) const {
	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "func") {
		printf("error: dialect '%s' not supported for translation from chp to flow.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int flowKind = weaver::Term::getDialect("__flow__");
	int flowIdx = prgm.createModule("__flow__");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: function must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the flow module
	string name = prgm.mods[modIdx].name + "_" + decl.name;
	// TODO(edward.bingham) merge weaver type system and flow types?
	vector<weaver::Instance> args = decl.args;

	int dstIdx = prgm.mods[flowIdx].createTerm(weaver::Term::procOf(flowKind, name, args));

	// Do the synthesis
	chp::graph &g = std::any_cast<chp::graph&>(prgm.mods[modIdx].terms[termIdx].def);
	g.post_process();	

	//string graph_render_filename = "_" + prefix + "_" + g.name + ".png";
	//gvdot::render(graph_render_filename, chp::export_graph(g, true).to_string());

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to flow
	}

	prgm.mods[flowIdx].terms[dstIdx].def = chp::synthesizeFuncFromCHP(g);
	return true;
}

bool Build::flowToVerilog(weaver::Program &prgm, int modIdx, int termIdx) const {
	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "__flow__") {
		printf("error: dialect '%s' not supported for translation from flow to val-rdy.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int verilogKind = weaver::Term::getDialect("__verilog__");
	int verilogIdx = prgm.createModule("__verilog__");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: flow must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the verilog module
	string name = prgm.mods[modIdx].name + "_" + decl.name;
	// TODO(edward.bingham) merge weaver type system and verilog types?
	vector<weaver::Instance> args = decl.args;

	int dstIdx = prgm.mods[verilogIdx].createTerm(weaver::Term::procOf(verilogKind, name, args));

	// Do the synthesis
	flow::Func &fn = std::any_cast<flow::Func&>(prgm.mods[modIdx].terms[termIdx].def);

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to verilog
	}

	prgm.mods[verilogIdx].terms[dstIdx].def = flow::synthesizeModuleFromFunc(fn);
	return true;
}

bool Build::hseToPrs(weaver::Program &prgm, int modIdx, int termIdx) const {
	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "proto") {
		printf("error: dialect '%s' not supported for translation from hse to prs.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int cktKind = weaver::Term::getDialect("ckt");
	int cktIdx = prgm.createModule("__ckt__");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: protocol must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = prgm.mods[modIdx].name + "_" + decl.name;
	vector<weaver::Instance> args = decl.args;

	int dstIdx = prgm.mods[cktIdx].createTerm(weaver::Term::procOf(cktKind, name, args));

	hse::graph &hg = std::any_cast<hse::graph&>(prgm.mods[modIdx].terms[termIdx].def);
	hg.name = decl.name;
	hg.post_process(true);
	hg.check_variables();

	if (progress) printf("Elaborate state space:\n");
	hse::elaborate(hg, stage >= Build::ENCODE or not noGhosts, true, progress);
	if (progress) printf("done\n\n");

	if (not is_clean()) {
		return false;
	}

	if (not get(Build::CONFLICTS)) {
		return true;
	}

	bool inverting = false;
	if (logic == Build::LOGIC_CMOS) {
		inverting = true;
	}

	hse::encoder enc;
	enc.base = &hg;

	if (progress) printf("Identify state conflicts:\n");
	enc.check(!inverting, progress);
	if (progress) printf("done\n\n");

	if (has(Build::CONFLICTS)) {
		print_conflicts(enc);
	}

	if (not get(Build::ENCODE)) {
		return true;
	}

	if (progress) printf("Insert state variables:\n");
	if (not enc.insert_state_variables(20, !inverting, progress, false)) {
		return false;
	}
	if (progress) printf("done\n\n");

	if (enc.conflicts.size() > 0) {
		// state variable insertion failed
		print_conflicts(enc);
		return false;
	}

	if (not get(Build::RULES)) {
		return true;
	}

	if (progress) printf("Synthesize production rules:\n");
	prs::production_rule_set pr;
	hse::synthesize_rules(&pr, &hg, !inverting, progress);
	if (progress) printf("done\n\n");

	prgm.mods[cktIdx].terms[dstIdx].def = pr;
	return true;
}

bool Build::prsToSpi(weaver::Program &prgm, int modIdx, int termIdx) {
	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "ckt") {
		printf("error: dialect '%s' not supported for translation from prs to spi.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int spiKind = weaver::Term::getDialect("__spice__");
	int spiIdx = prgm.createModule("__spice__");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: circuit must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = prgm.mods[modIdx].name + "_" + decl.name;
	vector<weaver::Instance> args = decl.args;

	int dstIdx = prgm.mods[spiIdx].createTerm(weaver::Term::procOf(spiKind, name, args));

	prs::production_rule_set &pr = std::any_cast<prs::production_rule_set&>(prgm.mods[modIdx].terms[termIdx].def);

	bool inverting = false;
	if (logic == Build::LOGIC_CMOS) {
		inverting = true;
	}

	if (inverting and not pr.cmos_implementable()) {
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

	if (not get(Build::KEEPERS)) {
		return true;
	}

	if (logic == Build::LOGIC_CMOS or logic == Build::LOGIC_RAW) {
		if (progress) printf("Insert keepers:\n");
		pr.add_keepers(true, false, 1, progress);
		if (progress) printf("done\n\n");
	}

	if (not get(Build::SIZE)) {
		return true;
	}

	if (progress) printf("Size production rules:\n");
	pr.size_devices(0.1, progress);
	if (progress) printf("done\n\n");

	if (tech.path.empty() and not phy::loadTech(tech, techPath, cellsDir)) {
		cout << "Unable to load techfile \'" + techPath + "\'." << endl;
		return false;
	}
	
	sch::Netlist net;
	if (progress) printf("Build netlist:\n");
	net.subckts.push_back(prs::build_netlist(tech, pr, progress));
	if (progress) printf("done\n\n");
	if (debug) {
		net.subckts.back().print();
	}

	prgm.mods[spiIdx].terms[dstIdx].def = net;
	return true;
}

bool Build::spiToGds(weaver::Program &prgm, int modIdx, int termIdx) {
	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "__spice__") {
		printf("error: dialect '%s' not supported for translation from spice to gds.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int gdsKind = weaver::Term::getDialect("__layout__");
	int gdsIdx = prgm.createModule("__layout__");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: spice must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = prgm.mods[modIdx].name + "_" + decl.name;
	vector<weaver::Instance> args = decl.args;

	int dstIdx = prgm.mods[gdsIdx].createTerm(weaver::Term::procOf(gdsKind, name, args));

	sch::Netlist &net = std::any_cast<sch::Netlist&>(prgm.mods[modIdx].terms[termIdx].def);

	if (noCells) {
		for (int i = 0; i < (int)net.subckts.size(); i++) {
			net.subckts[i].isCell = true;
		}
	}

	if (tech.path.empty() and not phy::loadTech(tech, techPath, cellsDir)) {
		cout << "Unable to load techfile \'" + techPath + "\'." << endl;
		return false;
	}

	if (progress) printf("Break subckts into cells:\n");
	Timer cellsTmr;
	net.mapCells(tech, progress);
	if (progress) printf("done\t%gs\n\n", cellsTmr.since());

	if (not get(Build::CELLS)) {
		return true;
	}

	phy::Library lib(tech);

	map<int, gdstk::Cell*> cells;

	//gdstk::GdsWriter gds = {};
	//gds = gdstk::gdswriter_init((name+".gds").c_str(), name.c_str(), ((double)tech.dbunit)*1e-6, ((double)tech.dbunit)*1e-6, 4, nullptr, nullptr);
	
	cell::update_library(lib, net, nullptr, &cells, progress, debug);

	if (not get(Build::PLACE)) {
		//gds.close();
		prgm.mods[gdsIdx].terms[dstIdx].def = lib;
		return true;
	}

	doPlacement(lib, net, nullptr, &cells, progress);

	if (not get(Build::ROUTE)) {
		//gds.close();
		prgm.mods[gdsIdx].terms[dstIdx].def = lib;
		return true;
	}

	//gds.close();
	prgm.mods[gdsIdx].terms[dstIdx].def = lib;
	return true;
}

bool Build::emitVerilog(string path, const weaver::Program &prgm, int modIdx, int termIdx) const {
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "__verilog__") {
		printf("internal:%s:%d: expected clocked module.\n", __FILE__, __LINE__);
		return false;
	}

	std::filesystem::create_directories(path);
	
	string filename = path+"/"+prgm.mods[modIdx].terms[termIdx].decl.name+".v";
	FILE *fptr = fopen(filename.c_str(), "w");
	if (fptr == nullptr) {
		printf("error: unable to write to file '%s'\n", filename.c_str());
		return false;
	}
	
	const clocked::Module &mod = std::any_cast<const clocked::Module&>(prgm.mods[modIdx].terms[termIdx].def);
	fprintf(fptr, "%s\n", flow::export_module(mod).to_string().c_str());

	fclose(fptr);
	return true;
}

bool Build::emitPrs(string path, const weaver::Program &prgm, int modIdx, int termIdx) const {
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "ckt") {
		printf("internal:%s:%d: expected production rule set.\n", __FILE__, __LINE__);
		return false;
	}

	std::filesystem::create_directories(path);
	
	string filename = path+"/"+prgm.mods[modIdx].terms[termIdx].decl.name+".prs";
	FILE *fptr = fopen(filename.c_str(), "w");
	if (fptr == nullptr) {
		printf("error: unable to write to file '%s'\n", filename.c_str());
		return false;
	}
	
	const prs::production_rule_set &pr = std::any_cast<const prs::production_rule_set&>(prgm.mods[modIdx].terms[termIdx].def);
	fprintf(fptr, "%s\n", prs::export_production_rule_set(pr).to_string().c_str());

	fclose(fptr);
	return true;
}

bool Build::emitSpice(string path, const weaver::Program &prgm, int modIdx, int termIdx) const {
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "__spice__") {
		printf("internal:%s:%d: expected spice netlist.\n", __FILE__, __LINE__);
		return false;
	}

	std::filesystem::create_directories(path);
	
	string filename = path+"/"+prgm.mods[modIdx].terms[termIdx].decl.name+".spi";
	FILE *fptr = fopen(filename.c_str(), "w");
	if (fptr == nullptr) {
		printf("error: unable to write to file '%s'\n", filename.c_str());
		return false;
	}
	
	const sch::Netlist &net = std::any_cast<const sch::Netlist&>(prgm.mods[modIdx].terms[termIdx].def);
	fprintf(fptr, "%s\n", sch::export_netlist(tech, net).to_string().c_str());

	fclose(fptr);
	return true;
}

bool Build::emitGds(string path, const weaver::Program &prgm, int modIdx, int termIdx) const {
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "__layout__") {
		printf("internal:%s:%d: expected layout.\n", __FILE__, __LINE__);
		return false;
	}

	std::filesystem::create_directories(path);
	
	string name = prgm.mods[modIdx].terms[termIdx].decl.name;
	const phy::Library &lib = std::any_cast<const phy::Library&>(prgm.mods[modIdx].terms[termIdx].def);
	phy::export_library(name, path+"/"+name+".gds", lib);
	return true;
}

