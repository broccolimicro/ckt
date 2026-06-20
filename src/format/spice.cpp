#include "spice.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_spice/factory.h>
#include <sch/Subckt.h>
#include <phy/Tech.h>
#include <phy/Script.h>

#include <interpret_sch/import.h>
#include <interpret_sch/export.h>

#include "../back/asic.h"

void readSpice(weaver::Project &proj, weaver::Source &source, string buffer) {
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	parse_spice::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);

	source.tokens->increment(false);
	parse_spice::expect(*source.tokens);
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_spice::netlist(*source.tokens));
	}
}

void loadSpice(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	string name = source.path.stem().string();
	std::vector<sch::Subckt> lst;
	sch::import_netlist(*tech, lst, *(parse_spice::netlist*)source.syntax.get(), source.tokens.get());

	for (auto i = lst.begin(); i != lst.end(); i++) {
		weaver::TermId id;
		id.mod   = prgm.getModule(source.modName);
		id.index = prgm.modAt(id).createTerm(weaver::Term(name, vector<weaver::Instance>()));
		id.var   = prgm.termAt(id).createVariant(weaver::Variant("spice", *i));
	}
}

void writeSpice(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx) {
	static std::set<std::string> prev;

	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	// If we keep updating a single file, then we don't want to have to read
	// the whole file over again. However, on each compile, we do want to
	// obliterate old build files.
	string pathstr = path.string();
	ofstream fout;
	if (prev.find(pathstr) != prev.end() and fs::exists(path)) {
		fout = ofstream(pathstr.c_str(), ios::out | ios::ate);
	} else {
		fout = ofstream(pathstr.c_str(), ios::out);
		prev.insert(pathstr);
	}

	if (not fout.is_open()) {
		error("", "unable to write to file '" + pathstr + "'", __FILE__, __LINE__);
		return;
	}

	const sch::Subckt &ckt = prgm.mods[modIdx].terms[termIdx].variants[varIdx].as<sch::Subckt>();
	string buffer = sch::export_subckt(*tech, ckt).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}
