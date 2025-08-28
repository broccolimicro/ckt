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

void readGds(weaver::Project &proj, weaver::Program &prgm, string path, string buffer) {
}

void writeGds(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	string name = prgm.mods[modIdx].terms[termIdx].decl.name;
	const phy::Library &lib = std::any_cast<const phy::Library&>(prgm.mods[modIdx].terms[termIdx].def);
	phy::export_library(name, path.string(), lib);
}
