#include "spice.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_spice/factory.h>
#include <sch/Netlist.h>
#include <phy/Tech.h>
#include <phy/Script.h>

#include <interpret_sch/import.h>
#include <interpret_sch/export.h>

void readSpice(weaver::Project &proj, weaver::Source &source, string buffer) {
	if (not proj.tech.def.has_value()) {
		proj.tech.def = make_any<phy::Tech>();
	}
	phy::Tech &tech = proj.tech.as<phy::Tech>();
	if (not tech.isLoaded() and not phy::loadTech(&tech, proj.tech.path, proj.tech.args)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
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
	if (not proj.tech.def.has_value()) {
		proj.tech.def = make_any<phy::Tech>();
	}
	phy::Tech &tech = proj.tech.as<phy::Tech>();
	if (not tech.isLoaded() and not phy::loadTech(&tech, proj.tech.path, proj.tech.args)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return;
	}

	string name = source.path.stem().string();
	sch::Netlist net;
	sch::import_netlist(tech, net, *(parse_spice::netlist*)source.syntax.get(), source.tokens.get());

	int kind = weaver::Term::getDialect("spice");
	int modIdx = prgm.getModule(source.modName);

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term(name, vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].variants.push_back(weaver::Variant(-1, net, weaver::Metadata(kind)));
}

void writeSpice(fs::path path, weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx) {
	if (not proj.tech.def.has_value()) {
		proj.tech.def = make_any<phy::Tech>();
	}
	phy::Tech &tech = proj.tech.as<phy::Tech>();
	if (not tech.isLoaded() and not phy::loadTech(&tech, proj.tech.path, proj.tech.args)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return;
	}

	string pathstr = path.string();
	ofstream fout(pathstr.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", pathstr.c_str());
		return;
	}

	const sch::Netlist &net = prgm.mods[modIdx].terms[termIdx].variants[varIdx].as<sch::Netlist>();
	string buffer = sch::export_netlist(tech, net).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}
