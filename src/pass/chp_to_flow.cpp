#include "chp_to_flow.h"

#include <chp/synthesize.h>
#include <gc/guarded_command.h>

bool flatten(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "func") {
		return false;
	}
	chp::graph &g = prgm.varAt(id).as<chp::graph>();

	if (g.isFlat()) {
		return true;
	} else if (prgm.varAt(id).meta.has("func.flatten")) {
		return false;
	}

	g.flatten(builder.debug);
	prgm.varAt(id).meta.set("func.flatten");
	return g.isFlat();
}

bool decompose(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "func") {
		return false;
	}
	if (prgm.varAt(id).meta.has("func.decompose")) {
		return false;
	}

	chp::graph &g = prgm.varAt(id).as<chp::graph>();

	vector<chp::graph> sub = g.decompose();
	if (sub.size() <= 1) {
		prgm.varAt(id).meta.set("func.decompose");
		return false;
	}

	int kind = prgm.varAt(id).meta.kind;
	id.var = prgm.termAt(id).createVariant(weaver::Variant("struct", std::any(), id.var));
	builder.todo.push_back(id);

	gc::GuardedCommands rules;
	for (size_t pid = 0; pid < sub.size(); pid++) {
		vector<weaver::Instance> args = prgm.termAt(id).decl.args;
		for (int i = (int)args.size()-1; i >= 0; i--) {
			if (g.netIndex(args[i].name) < 0) {
				args.erase(args.begin()+i);
			}
		}

		weaver::TermId subId = prgm.createTerm(id.mod, weaver::Term(sub[pid].name, args));
		subId.var = prgm.termAt(subId).createVariant(weaver::Variant(kind, sub[pid]));
		builder.todo.push_back(subId);

		rules.rules.push_back(gc::GuardedCommand(arithmetic::Choice({arithmetic::Parallel({arithmetic::Action(arithmetic::call(prgm.termAt(subId).decl.name, {}))})})));
	}
	prgm.varAt(id).set(rules);

	return true;
}

bool chpToFlow(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect() != "func") {
		return false;
	}
	chp::graph &g = prgm.varAt(id).as<chp::graph>();

	if (not g.isFlat()) {
		return false;
	}

	auto tmpl = chp::synthesizeFuncFromCHP(g, builder.debug);
	
	id.var = prgm.termAt(id).createVariant(weaver::Variant("flow", tmpl, id.var));
	builder.todo.push_back(id);
	return true;
}
