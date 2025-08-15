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

#include <filesystem>

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

/*bool canonicalize_hse(const Build &builder, weaver::Module &tbl, Function &func) {
	func.hg->post_process(true);
	func.hg->check_variables();

	if (doPostprocess) {
		export_astg(func.hg->name+"_post.astg", *func.hg);
	}
	return true;
}

bool elaborate_hse(const Build &builder, weaver::Module &tbl, Function &func) {
	if (progress) printf("Elaborate state space:\n");
	hse::elaborate(*func.hg, stage >= Build::ENCODE or not noGhosts, true, progress);
	if (progress) printf("done\n\n");
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

	if (has(Build::CONFLICTS)) {
		print_conflicts(enc);
	}

	if (not get(Build::ENCODE)) {
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

	if (has(Build::ENCODE)) {
		string suffix = stage == Build::ENCODE ? "" : "_complete";
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

void Build::build(weaver::Program &prgm) {
	for (int i = 0; i < (int)prgm.mods.size(); i++) {
		for (int j = 0; j < (int)prgm.mods[i].terms.size(); j++) {
			string dialectName = prgm.mods[i].terms[j].dialect().name;
			if (dialectName == "func") {
				chpToFlow(prgm, i, j);
			} else if (dialectName == "__flow__") {
				flowToValRdy(prgm, i, j);
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
	if (dialectName == "__valrdy__") {
		return "rtl";
	} else if (dialectName == "__spi__") {
		return "spi";
	} else if (dialectName == "__gds__") {
		return "gds";
	} else if (dialectName == "ckt") {
		return "ckt";
	}
	return "dbg";
}

void Build::emit(string path, const weaver::Program &prgm) const {
	for (int i = 0; i < (int)prgm.mods.size(); i++) {
		for (int j = 0; j < (int)prgm.mods[i].terms.size(); j++) {
			emit(path+"/"+getBuildDir(prgm.mods[i].terms[j].dialect().name), prgm, i, j);
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

bool Build::flowToValRdy(weaver::Program &prgm, int modIdx, int termIdx) const {
	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "__flow__") {
		printf("error: dialect '%s' not supported for translation from flow to val-rdy.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int valrdyKind = weaver::Term::getDialect("__valrdy__");
	int valrdyIdx = prgm.createModule("__valrdy__");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: function must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the valrdy module
	string name = prgm.mods[modIdx].name + "_" + decl.name;
	// TODO(edward.bingham) merge weaver type system and valrdy types?
	vector<weaver::Instance> args = decl.args;

	int dstIdx = prgm.mods[valrdyIdx].createTerm(weaver::Term::procOf(valrdyKind, name, args));

	// Do the synthesis
	flow::Func &fn = std::any_cast<flow::Func&>(prgm.mods[modIdx].terms[termIdx].def);

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to valrdy
	}

	prgm.mods[valrdyIdx].terms[dstIdx].def = flow::synthesizeModuleFromFunc(fn);
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
		printf("error: function must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the module
	string name = prgm.mods[modIdx].name + "_" + decl.name;
	vector<weaver::Instance> args = decl.args;

	int dstIdx = prgm.mods[cktIdx].createTerm(weaver::Term::procOf(cktKind, name, args));

	hse::graph &hg = std::any_cast<hse::graph&>(prgm.mods[modIdx].terms[termIdx].def);

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
		printf("error: function must be a full process for synthesis\n");
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
}

bool Build::emit(string path, const weaver::Program &prgm, int modIdx, int termIdx) const {
	std::filesystem::create_directories(path);
	
	if (prgm.mods[modIdx].terms[termIdx].dialect().name == "__valrdy__") {
		string filename = path+"/"+prgm.mods[modIdx].terms[termIdx].decl.name+".v";
		FILE *fptr = fopen(filename.c_str(), "w");
		if (fptr == nullptr) {
			printf("error: unable to write to file '%s'\n", filename.c_str());
			return false;
		}
		
		const clocked::Module &mod = std::any_cast<const clocked::Module&>(prgm.mods[modIdx].terms[termIdx].def);
		fprintf(fptr, "%s\n", flow::export_module(mod).to_string().c_str());

		fclose(fptr);
	}
	return true;
}


