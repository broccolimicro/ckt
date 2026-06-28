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

#include <weaver/params.h>

weaver::Decl declFromSubckt(const weaver::Program &prgm, int mod, const sch::Subckt &ckt) {
	std::map<std::string, std::string> params = weaver::readParams({ckt.comment});
	auto pos = params.find("decl");
	if (pos != params.end()) {
		return prgm.findDecl(weaver::Prototype(pos->second), mod);
	}

	weaver::TypeId wireType(prgm.global, prgm.mods[prgm.global].findType("wire"));

	weaver::Decl decl;
	decl.name = ckt.name;

	// All of the ports in a cell are wires
	vector<weaver::Instance> args;
	for (int j : ckt.ports) {
		decl.args.push_back(weaver::Instance(wireType, ckt.nets[j].name));
	}
	return decl;
}

weaver::Prototype protoFromInstance(const weaver::Program &prgm, int mod, const sch::Instance &inst, bool qualify) {
	std::map<std::string, std::string> params = weaver::readParams({inst.comment});
	auto pos = params.find("proto");
	if (pos != params.end()) {
		return weaver::Prototype(pos->second);
	}
	weaver::Prototype proto; // = weaver::Prototype::fromMangled(inst.type);
	proto.name = inst.type;

	// TODO(edward.bingham) better to leave unqualified?
	if (qualify) {
		proto.qualified = true;
		for (int j : inst.ports) {
			proto.args.push_back(weaver::Typename("wire"));
		}
	}
	return proto;
}

void readSpice(weaver::Project &proj, weaver::Source &source, string buffer) {
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	parse_spice::netlist::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);

	source.tokens->increment(false);
	source.tokens->expect<parse_spice::netlist>();
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_spice::netlist(*source.tokens));
	}
}

void loadSpice(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return;
	}

	weaver::TypeId wireType(prgm.global, prgm.mods[prgm.global].findType("wire"));

	std::vector<sch::Subckt> lst;
	sch::import_netlist(*tech, lst, *(parse_spice::netlist*)source.syntax.get(), source.tokens.get());

	for (auto &ckt : lst) {
		int mod = prgm.getModule(source.modName);
		weaver::Decl decl = declFromSubckt(prgm, mod, ckt);
		
		weaver::TermId id = prgm.getTerm(mod, decl);
		if (prgm.termValid(id)) {
			auto &term = prgm.termAt(id);
			id.var   = term.createVariant(weaver::Variant("spice", ckt));
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
			internal("", "term not defined '" + prgm.getPrototype(decl, source.modName).to_string() + "'", __FILE__, __LINE__);
		}
	}
}

void writeSpice(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id) {
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

	const sch::Subckt &ckt = prgm.varAt(id).as<sch::Subckt>();
	string buffer = sch::export_subckt(*tech, ckt).to_string();
	fwrite(buffer.c_str(), sizeof(char), buffer.size(), fptr);
	fclose(fptr);
}

std::vector<weaver::Prototype> linkSpice(const weaver::Project &proj, const weaver::Program &prgm, weaver::TermId id) {
	sch::Subckt ckt = prgm.varAt(id).as<sch::Subckt>();

	std::vector<weaver::Prototype> result;
	for (const sch::Instance &inst : ckt.inst) {
		result.push_back(protoFromInstance(prgm, id.mod, inst, true));
	}
	return result;
}
