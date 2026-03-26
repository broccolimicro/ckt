#include "chp_to_flow.h"

#include <chp/synthesize.h>
#include <gc/guarded_command.h>

bool flatten(Build &builder, weaver::Program &prgm, weaver::TermId id, int index) {
	weaver::Term &term = prgm.termAt(id);
	weaver::Variant &var = term.variants[index];
	if (var.meta.dialect() != "func") {
		return false;
	}
	chp::graph &g = var.as<chp::graph>();

	if (g.isFlat()) {
		return true;
	} else if (term.variants[index].meta.has("func.flatten")) {
		return false;
	}

	g.flatten(builder.debug);
	var.meta.set("func.flatten");
	return g.isFlat();
}

bool decompose(Build &builder, weaver::Program &prgm, weaver::TermId id, int index) {
	weaver::Term &term = prgm.termAt(id);
	weaver::Variant &var = term.variants[index];
	if (var.meta.dialect() != "func") {
		return false;
	}
	if (term.variants[index].meta.has("func.decompose")) {
		return false;
	}

	chp::graph &g = var.as<chp::graph>();

	vector<chp::graph> sub = g.decompose();
	if (sub.size() <= 1) {
		var.meta.set("func.decompose");
		return false;
	}

	gc::GuardedCommands rules;
	for (size_t pid = 0; pid < sub.size(); pid++) {
		vector<weaver::Instance> args = term.decl.args;
		for (int i = (int)args.size()-1; i >= 0; i--) {
			if (g.netIndex(args[i].name) < 0) {
				args.erase(args.begin()+i);
			}
		}

		weaver::TermId subId = prgm.createTerm(id.mod, weaver::Term(term.decl.name + "_" + std::to_string(pid), args));
		weaver::Term &subTerm = prgm.termAt(subId);
		subTerm.variants.push_back(weaver::Variant(-1, sub[pid], weaver::Metadata(var.meta.kind)));
		//builder.todo.push_back(subId);

		rules.rules.push_back(gc::GuardedCommand(arithmetic::Choice({arithmetic::Parallel({arithmetic::Action(arithmetic::call(subTerm.decl.name, {}))})})));
	}

	var.derived.push_back(term.variants.size());
	term.variants.push_back(weaver::Variant(index, rules, weaver::Metadata(weaver::Term::findDialect("struct"))));
	return true;
}

bool bind(Build &builder, weaver::Program &prgm, weaver::TermId id, int index) {
	weaver::Term &term = prgm.termAt(id);
	weaver::Variant &var = term.variants[index];
	if (var.meta.dialect() != "func") {
		return false;
	}
	chp::graph &g = var.as<chp::graph>();

	if (not g.isFlat()) {
		return false;
	}

	auto tmpl = chp::synthesizeFuncFromCHP(g, builder.debug);
	
	var.derived.push_back(term.variants.size());
	term.variants.push_back(weaver::Variant(index, tmpl, weaver::Metadata(weaver::Term::findDialect("flow"))));
	return true;
}

bool chpToFlow(Build &builder, weaver::Program &prgm, weaver::TermId &id, int index) {
	if (flatten(builder, prgm, id, index)) {
		return bind(builder, prgm, id, index);
	}

	if (not decompose(builder, prgm, id, index)) {
		return false;
	}


	return bind(builder, prgm, id, index);
}
