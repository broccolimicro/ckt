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
	if (not proj.tech.isLoaded() and not phy::loadTech(proj.tech)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return;
	}

	parse_spice::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);

	source.tokens->increment(false);
	parse_spice::expect(*source.tokens);
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = unique_ptr<parse::syntax>(new parse_spice::netlist(*source.tokens));
	}
}

void loadSpice(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	sch::Netlist net;
	sch::import_netlist(proj.tech, net, *(parse_spice::netlist*)source.syntax.get(), source.tokens.get());

	int kind = weaver::Term::getDialect("spice");
	int modIdx = prgm.getModule(source.modName.string());

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, source.modName.stem().string(), vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = net;
}

void writeSpice(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	ofstream fout(path.string().c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", path.string().c_str());
		return;
	}

	const sch::Netlist &net = std::any_cast<const sch::Netlist&>(prgm.mods[modIdx].terms[termIdx].def);
	string buffer = sch::export_netlist(proj.tech, net).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}
