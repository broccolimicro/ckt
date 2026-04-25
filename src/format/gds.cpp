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

#include "../back/asic.h"

void loadGds(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	string name = source.path.stem().string();
	phy::Library lib(*tech);
	import_library(lib, source.path.string());

	weaver::TermId id;
	id.mod   = prgm.getModule(source.modName);
	id.index = prgm.modAt(id).createTerm(weaver::Term(name, vector<weaver::Instance>()));
	id.var   = prgm.termAt(id).createVariant(weaver::Variant("layout", lib));
}

void writeGds(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx) {
	string name = prgm.mods[modIdx].terms[termIdx].decl.name;
	const phy::Library &lib = prgm.mods[modIdx].terms[termIdx].variants[varIdx].as<phy::Library>();
	phy::export_library(name, path.string(), lib);
}
