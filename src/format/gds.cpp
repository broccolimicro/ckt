#include "gds.h"
#include "param.h"

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

void guessPorts(weaver::Prototype &proto, const phy::Layout &macro) {
	// All of the ports in a cell are wires
	weaver::Typename wireType("wire");
	for (const auto &net : macro.nets) {
		if (net.isInput or net.isOutput) {
			proto.args.push_back(wireType);
		}
	}
	proto.hashArgs();
}

std::vector<weaver::TermId> loadGds(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	std::vector<weaver::TermId> result;
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return result;
	}

	string name = source.path.stem().string();
	std::vector<phy::Layout> lib;
	import_library(lib, *tech, source.path.string());

	for (auto macro = lib.begin(); macro != lib.end(); macro++) {
		weaver::Prototype proto = weaver::Prototype::fromMangled(macro->name);
		if (proto.mod.empty()) {
			proto.mod = source.modName;
		}
		int mod = prgm.getModule(proto.mod);

		weaver::TermId id;

		// First, check the metadata to see if we can extract the type information
		weaver::Decl decl = declFromParam(prgm, macro->properties, "wv.", mod);
		if (not decl.name.empty()) {
			id = prgm.getTerm(mod, decl);
		}

		// Then, try to find the prototype in the program
		if (not id.hasTerm()) {
			// fall back to the port list if need be
			//if (not proto.qualified) {
			//	macro->trace();
			//	guessPorts(proto, *macro);
			//}
			id = prgm.getTerm(proto, mod);
		}

		if (prgm.termValid(id)) {
			auto &term = prgm.termAt(id);
			id.var = term.createVariant(weaver::Variant("layout", *macro));
			prgm.varAt(id).fromSource = true;
			// look for the parent
			for (int i = id.var-1; i >= 0; i--) {
				if (term.variants[i].meta.dialect == "spice") {
					term.variants[id.var].super = i;
					term.variants[i].derived.push_back(id.var);
					break;
				}
			}

			result.push_back(id);
		} else {
			internal("", "term not defined '" + proto.to_string() + "'", __FILE__, __LINE__);
		}
	}
	return result;
}

void writeGds(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id) {
	static std::map<std::string, gdstk::GdsWriter> prev;

	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}
	
	const phy::Layout &macro = prgm.varAt(id).as<phy::Layout>();

	// If we keep updating a single file, then we don't want to have to read
	// the whole file over again. However, on each compile, we do want to
	// obliterate old build files.
	// max_points = 0 -> don't fracture polygons as we write them to the file
	string pathstr = path.string();
	auto pos = prev.insert({pathstr, gdstk::GdsWriter{nullptr, 0.0, 0.0, 0}});
	auto &writer = pos.first->second;
	if (not pos.second and fs::exists(path)) {
		writer.out = fopen(pathstr.c_str(), "r+b");
		fseek(writer.out, -2*sizeof(uint16_t), SEEK_END);
	} else {
		std::string name = path.stem();
		writer = gdstk::gdswriter_init(pathstr.c_str(), name.c_str(), ((double)tech->dbunit)*1e-6, ((double)tech->dbunit)*1e-6, 0, nullptr, nullptr);
	}

	if (writer.out == nullptr) {
		// If the file is corrupted, we don't want to obliterate it, the user may
		// want to recover it.
		error("", "unable to update gds file", __FILE__, __LINE__);
	}

	export_layout(writer, macro);
	writer.close();
}
