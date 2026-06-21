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

	std::vector<sch::Subckt> lst;
	sch::import_netlist(*tech, lst, *(parse_spice::netlist*)source.syntax.get(), source.tokens.get());

	for (auto ckt = lst.begin(); ckt != lst.end(); ckt++) {
		weaver::Prototype proto = weaver::Prototype::fromMangled(ckt->name);
		proto.mod = source.modName;

		weaver::TermId id = prgm.getTerm(proto);
		if (prgm.termValid(id)) {
			auto &term = prgm.termAt(id);
			id.var   = term.createVariant(weaver::Variant("spice", *ckt));
			// Look for the parent
			for (int i = id.var-1; i >= 0; i--) {
				if (term.variants[i].meta.dialect == "prs") {
					term.variants[id.var].super = i;
					term.variants[i].derived.push_back(id.var);
					break;
				}
			}
			// look for children
			for (int i = id.var-1; i >= 0; i--) {
				if (term.variants[i].super < 0 and term.variants[i].meta.dialect == "layout") {
					term.variants[i].super = id.var;
					term.variants[id.var].derived.push_back(i);
				}
			}
		} else {
			internal("", "term not defined '" + proto.to_string() + "'", __FILE__, __LINE__);
		}
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
	FILE *fptr = nullptr;
	if (prev.insert(pathstr).second and fs::exists(path)) {
		fptr = fopen(pathstr.c_str(), "w");
	} else {
		fptr = fopen(pathstr.c_str(), "a");
	}

	if (fptr == nullptr) {
		error("", "unable to write to file '" + pathstr + "'", __FILE__, __LINE__);
		return;
	}

	const sch::Subckt &ckt = prgm.mods[modIdx].terms[termIdx].variants[varIdx].as<sch::Subckt>();
	string buffer = sch::export_subckt(*tech, ckt).to_string();
	fwrite(buffer.c_str(), sizeof(char), buffer.size(), fptr);
	fclose(fptr);
}
