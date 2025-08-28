#include "gds.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Library.h>

#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

void loadGds(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	if (not proj.tech.isLoaded() and not phy::loadTech(proj.tech)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return;
	}

	phy::Library lib(proj.tech);
	import_library(lib, source.path.string());

	int kind = weaver::Term::getDialect("layout");
	int modIdx = prgm.getModule(source.modName.string());

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, source.modName.stem().string(), vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = lib;
}

void writeGds(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	string name = prgm.mods[modIdx].terms[termIdx].decl.name;
	const phy::Library &lib = std::any_cast<const phy::Library&>(prgm.mods[modIdx].terms[termIdx].def);
	phy::export_library(name, path.string(), lib);
}
