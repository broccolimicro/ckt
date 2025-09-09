#include "unpacker.h"

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

#include <interpret_flow/export_verilog.h>
#include <interpret_hse/export_cli.h>
#include <interpret_hse/export.h>
#include <interpret_prs/export.h>
#include <interpret_sch/export.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

#include <filesystem>

#include "../format/cell.h"

Unpack::Unpack(weaver::Project &proj) : proj(proj) {
	stage = -1;

	targets.resize(SIZED+1, false);
}

Unpack::~Unpack() {
}

void Unpack::set(int target) {
	stage = stage < target ? target : stage;
	targets[target] = true;
}

bool Unpack::get(int target) const {
	return stage < 0 or stage >= target;
}

void Unpack::inclAll() {
	targets = vector<bool>(SIZED+1, true);
}

void Unpack::incl(int target) {
	targets[target] = true;
}

void Unpack::excl(int target) {
	targets[target] = false;
}

bool Unpack::has(int target) const {
	return targets[target];
}

void Unpack::unpack(weaver::Program &prgm) {
	for (int i = 0; i < (int)prgm.mods.size(); i++) {
		for (int j = 0; j < (int)prgm.mods[i].terms.size(); j++) {
			if (prgm.mods[i].terms[j].kind < 0) {
				printf("internal:%s:%d: dialect not defined for term '%s'\n", __FILE__, __LINE__, prgm.mods[i].terms[j].decl.name.c_str());
				continue;
			}
			string dialectName = prgm.mods[i].terms[j].dialect().name;
			if (dialectName == "layout") {
				gdsToSpi(prgm, i, j);
			} else if (dialectName == "spice") {
				spiToPrs(prgm, i, j);
			}
		}
	}
}

bool Unpack::gdsToSpi(weaver::Program &prgm, int modIdx, int termIdx) const {
	string debugDir = proj.rootDir / proj.BUILD / "dbg";

	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "layout") {
		printf("error: dialect '%s' not supported for translation from layout to spice.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int spiceKind = weaver::Term::getDialect("spice");
	int spiceIdx = prgm.getModule(prgm.mods[modIdx].name + ">>spice");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: function must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the spice module
	string name = decl.name;
	// TODO(edward.bingham) merge weaver type system and spice types?
	vector<weaver::Instance> args = decl.args;

	// Do the synthesis
	phy::Library &lib = prgm.mods[modIdx].terms[termIdx].as<phy::Library>();

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to spice
	}

	int dstIdx = prgm.mods[spiceIdx].createTerm(weaver::Term::procOf(spiceKind, name, args));

	sch::Netlist net;
	extract(net, lib);
	prgm.mods[spiceIdx].terms[dstIdx].def = net;
	return true;
}

bool Unpack::spiToPrs(weaver::Program &prgm, int modIdx, int termIdx) const {
	string debugDir = proj.rootDir / proj.BUILD / "dbg";

	// Verify expected format of the term
	if (prgm.mods[modIdx].terms[termIdx].dialect().name != "spice") {
		printf("error: dialect '%s' not supported for translation from spice to circ.\n",
			prgm.mods[modIdx].terms[termIdx].dialect().name.c_str());
		return false;
	}

	// Create dialect and module
	int circKind = weaver::Term::getDialect("circ");
	int circIdx = prgm.getModule(prgm.mods[modIdx].name + ">>circ");

	const weaver::Decl &decl = prgm.mods[modIdx].terms[termIdx].decl;
	if (decl.ret.defined() or decl.recv.defined()) {
		printf("error: spice must be a full process for synthesis\n");
		return false;
	}

	// Create the new term in the circ module
	string name = decl.name;
	// TODO(edward.bingham) merge weaver type system and circ types?
	vector<weaver::Instance> args = decl.args;

	if (not proj.tech.isLoaded() and not phy::loadTech(proj.tech)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return false;
	}

	// Do the synthesis
	sch::Netlist &net = prgm.mods[modIdx].terms[termIdx].as<sch::Netlist>();

	for (auto i = args.begin(); i != args.end(); i++) {
		// TODO(edward.bingham) pass the variable declarations over to circ
	}

	for (auto ckt = net.subckts.begin(); ckt != net.subckts.end(); ckt++) {
		int dstIdx = prgm.mods[circIdx].createTerm(weaver::Term::procOf(circKind, ckt->name, args));
		prgm.mods[circIdx].terms[dstIdx].def = prs::extract_rules(proj.tech, *ckt);
	}
	return true;
}

