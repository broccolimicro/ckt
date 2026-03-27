#include "hse_to_prs.h"

#include <hse/elaborator.h>
#include <hse/encoder.h>
#include <hse/synthesize.h>
#include <interpret_hse/export_cli.h>
#include <interpret_hse/export.h>

bool elaborate(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "proto") {
		return false;
	}
	if (prgm.varAt(id).meta.has("proto.states")) {
		return true;
	}

	hse::graph &g = prgm.varAt(id).as<hse::graph>();

	hse::elaborate(g, builder.stage >= Build::ENCODE or not builder.noGhosts, true, builder.progress); 
	prgm.varAt(id).meta.set("proto.states");	

	/*if (has(Build::ELAB)) {
		auto debugDir = builder.debugDir() / term.decl.name;
		std::filesystem::create_directories(debugDir);
		string suffix = builder.stage == Build::ELAB ? "" : "_predicate";
		string filename = debugDir / (term.decl.name+suffix+".astgw");
		hse::export_astg(filename, g);
	}*/

	return true;
}

bool conflicts(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "proto"
		or not prgm.varAt(id).meta.has("proto.states")) {
		return false;
	}
	if (prgm.varAt(id).meta.has("proto.conflicts")) {
		return true;
	}

	hse::graph &g = prgm.varAt(id).as<hse::graph>();

	auto enc = prgm.varAt(id).meta.set("proto.conflicts", hse::encoder(&g));
	enc->check(builder.logic != Build::LOGIC_CMOS, builder.progress);

	if (builder.has(Build::CONFLICTS)) {
		print_conflicts(*enc);
	}
	return true;
}

bool encode(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "proto"
		or not prgm.varAt(id).meta.has("proto.states")
		or not prgm.varAt(id).meta.has("proto.conflicts")) {
		return false;
	}
	auto enc = prgm.varAt(id).meta.get<hse::encoder>("proto.conflicts");
	if (enc->conflicts.empty()) {
		return false;
	}

	enc->insert_state_variable(builder.debug);
	prgm.varAt(id).meta.unset("proto.conflicts");
	prgm.varAt(id).meta.unset("proto.states");
	return true;
}

bool hseToPrs(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "proto"
		or not prgm.varAt(id).meta.has("proto.states")
		or not prgm.varAt(id).meta.has("proto.conflicts")) {
		return false;
	}
	auto enc = prgm.varAt(id).meta.get<hse::encoder>("proto.conflicts");
	if (not enc->conflicts.empty()) {
		return false;
	}

	hse::graph &g = prgm.varAt(id).as<hse::graph>();

	prs::production_rule_set pr;
	hse::synthesize_rules(&pr, &g, builder.logic != Build::LOGIC_CMOS, builder.progress);

	id.var = prgm.termAt(id).createVariant(weaver::Variant("circ", pr, id.var));
	builder.todo.push_back(id);
	return true;
}
