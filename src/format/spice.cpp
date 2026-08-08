#include "spice.h"
#include "param.h"

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

void guessPorts(weaver::Prototype &proto, const sch::Subckt &ckt) {
	// All of the ports in a cell are wires
	weaver::Typename wireType("wire");
	for (int j : ckt.ports) {
		proto.args.push_back(wireType);
	}
	proto.hashArgs();
}

weaver::Decl declFromSubckt(const weaver::Program &prgm, const sch::Subckt &ckt, std::string name) {
	weaver::TypeId wireType(prgm.global, prgm.mods[prgm.global].findType("wire"));
	weaver::Typename wireName("wire");

	weaver::Decl decl;
	decl.name = ckt.name;
	if (not name.empty()) {
		decl.name = name;
	}
	vector<weaver::Typename> args;
	for (int j : ckt.ports) {
		decl.args.push_back(weaver::Instance(wireType, ckt.nets[j].name));
		args.push_back(wireName);
	}
	decl.argsHash = weaver::getHash(args);
	decl.hashed = true;
	decl.qualified = true;
	return decl;
}

weaver::Prototype protoFromInstance(const weaver::Program &prgm, const sch::Instance &inst, bool qualify) {
	std::map<std::string, std::string> params = weaver::readParams({inst.comment});
	weaver::Prototype proto = protoFromParam(params);
	if (not proto.empty()) {
		return proto;
	}

	proto = weaver::Prototype::fromMangled(inst.type);

	if (qualify) {
		proto.qualified = true;
		for (int j : inst.ports) {
			proto.args.push_back(weaver::Typename("wire"));
		}
		proto.argsHash = weaver::getHash(proto.args);
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

std::vector<weaver::TermId> loadSpice(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	std::vector<weaver::TermId> result;
	phy::Tech *tech = loadASIC(proj);
	if (not tech) {
		return result;
	}

	weaver::TypeId wireType(prgm.global, prgm.mods[prgm.global].findType("wire"));

	std::vector<sch::Subckt> lst;
	sch::import_netlist(*tech, lst, *(parse_spice::netlist*)source.syntax.get(), source.tokens.get());

	for (auto &ckt : lst) {
		weaver::Prototype proto = weaver::Prototype::fromMangled(ckt.name);
		if (proto.mod.empty()) {
			proto.mod = source.modName;
		}
		int mod = prgm.getModule(proto.mod);

		weaver::TermId id;

		// First, check the metadata to see if we can extract the type information
		std::map<std::string, std::string> params = weaver::readParams({ckt.comment});
		if (not params.empty()) {
			weaver::Decl decl = declFromParam(prgm, params, "", mod);
			if (not decl.name.empty()) {
				id = prgm.getTerm(mod, decl);
			}
		}

		// Then, try to find the prototype in the program
		if (not id.hasTerm()) {
			// fall back to the port list if need be
			//if (not proto.qualified) {
			//	guessPorts(proto, *macro);
			//}
			id = prgm.getTerm(proto, mod);
		}

		if (prgm.termValid(id)) {
			auto &term = prgm.termAt(id);
			id.var   = term.createVariant(weaver::Variant("spice", ckt));
			prgm.varAt(id).fromSource = true;
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
			result.push_back(id);
		} else {
			internal("", "term not defined '" + proto.to_string() + "'", __FILE__, __LINE__);
		}
	}
	return result;
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
		result.push_back(protoFromInstance(prgm, inst, false));
	}
	return result;
}
