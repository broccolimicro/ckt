#include "gds.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Layout.h>

#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

#include "../back/asic.h"

void loadGds(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	string name = source.path.stem().string();
	std::vector<phy::Layout> lib;
	import_library(lib, *tech, source.path.string());

	for (auto macro = lib.begin(); macro != lib.end(); macro++) {
		weaver::Prototype proto(macro->name);
		vector<weaver::TermId> ids = prgm.findTerms(proto);
		
		if (ids.empty()) {
			int mod = prgm.getModule(proto.mod);
			vector<weaver::Instance> args;
			weaver::TypeId recv;
			if (not proto.unqualified) {
				// TODO(edward.bingham) add variable names by looking at ports
				for (auto arg = proto.args.begin(); arg != proto.args.end(); arg++) {
					args.push_back(prgm.findInstance(*arg, "", mod));
				}
				recv = prgm.findType("", proto.recv, mod);
			}

			int idx = prgm.mods[mod].createTerm(weaver::Term(proto.name, args, weaver::TypeId(), recv));
			ids.push_back(weaver::TermId(mod, idx));
		}

		if (ids.size() > 1u) {
			error("", "ambiguous process names", __FILE__, __LINE__);
		}
		weaver::TermId id = ids[0];
		id.var = prgm.termAt(id).createVariant(weaver::Variant("layout", *macro));
	}
}

void writeGds(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx) {
	static std::set<std::string> prev;

	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	// If we keep updating a single file, then we don't want to have to read
	// the whole file over again. However, on each compile, we do want to
	// obliterate old build files.
	// max_points = 0 -> don't fracture polygons as we write them to the file
	gdstk::GdsWriter writer{nullptr, 0.0, 0.0, 0};
	string pathstr = path.string();
	if (prev.find(pathstr) != prev.end() and fs::exists(path)) {
		writer = gdstk::GdsWriter{fopen(pathstr.c_str(), "ab"), ((double)tech->dbunit)*1e-6, ((double)tech->dbunit)*1e-6, 0};
	} else {
		std::string name = path.stem();
		writer = gdstk::gdswriter_init(pathstr.c_str(), name.c_str(), ((double)tech->dbunit)*1e-6, ((double)tech->dbunit)*1e-6, 0, nullptr, nullptr);
		fseek(writer.out, -2*sizeof(uint16_t), SEEK_END);
		prev.insert(pathstr);
	}

	if (writer.out == nullptr) {
		// If the file is corrupted, we don't want to obliterate it, the user may
		// want to recover it.
		error("", "unable to update gds file", __FILE__, __LINE__);
	}

	const phy::Layout &macro = prgm.mods[modIdx].terms[termIdx].variants[varIdx].as<phy::Layout>();

	writer.write_cell(*export_layout(macro));
	writer.close();
}
