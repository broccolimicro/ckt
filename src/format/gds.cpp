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
	if (not proj.tech.def.has_value()) {
		proj.tech.def = make_any<phy::Tech>();
	}
	phy::Tech &tech = proj.tech.as<phy::Tech>();
	if (not tech.isLoaded() and not phy::loadTech(&tech, proj.tech.path, proj.tech.args)) {
		cout << "Unable to load techfile \'" + proj.tech.path + "\'." << endl;
		return;
	}

	string name = source.path.stem().string();
	phy::Library lib(tech);
	import_library(lib, source.path.string());

	weaver::TermId id;
	id.mod   = prgm.getModule(source.modName);
	id.index = prgm.modAt(id).createTerm(weaver::Term(name, vector<weaver::Instance>()));
	id.var   = prgm.termAt(id).createVariant(weaver::Variant("layout", lib));
}

void writeGds(fs::path path, weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx) {
	string name = prgm.mods[modIdx].terms[termIdx].decl.name;
	const phy::Library &lib = prgm.mods[modIdx].terms[termIdx].variants[varIdx].as<phy::Library>();
	phy::export_library(name, path.string(), lib);
}
