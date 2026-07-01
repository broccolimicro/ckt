#include "chp_to_flow.h"

#include <chp/synthesize.h>
#include <gc/guarded_command.h>

#include <interpret_gc/export.h>

bool flatten(const Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "func") {
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
	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "func") {
		return false;
	}
	if (prgm.varAt(id).meta.has("func.decompose")) {
		return false;
	}

	chp::graph g = prgm.varAt(id).as<chp::graph>();

	vector<chp::graph> sub = g.decompose();
	if (sub.size() <= 1) {
		prgm.varAt(id).meta.set("func.decompose");
		return false;
	}

	string dialect = prgm.varAt(id).meta.dialect;
	id.var = prgm.termAt(id).createVariant(weaver::Variant("struct", gc::GuardedCommands(), id.var));
	gc::GuardedCommands &rules = prgm.varAt(id).as<gc::GuardedCommands>();
	for (size_t pid = 0; pid < sub.size(); pid++) {
		arithmetic::Action action(arithmetic::call(sub[pid].name, {}));
		arithmetic::Choice choice({{action}});
		cout << "LOOK GC ACTION '" << sub[pid].name << "' action=" << action << " choice=" << choice << endl;
		rules.rules.push_back(gc::GuardedCommand(choice));
		cout << "export=" << gc::export_rule(rules.rules.back(), rules).to_string() << endl;
	}
	for (size_t i = 0; i < g.vars.size(); i++) {
		rules.vars.push_back(gc::Variable(g.vars[i].name, g.vars[i].region));
		rules.vars.back().remote = g.vars[i].remote;
	}
	builder.todo.push_back(id);

	for (size_t pid = 0; pid < sub.size(); pid++) {
		vector<weaver::Instance> args;
		weaver::TermId subId = prgm.createTerm(id.mod, weaver::Term(sub[pid].name, args));
		subId.var = prgm.termAt(subId).createVariant(weaver::Variant(dialect, sub[pid]));
		builder.todo.push_back(subId);
	}

	return true;
}

bool chpToFlow(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar() or prgm.varAt(id).meta.dialect != "func") {
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
