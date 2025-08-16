#include "dialect.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>
#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>
#include <chp/graph.h>
#include <hse/graph.h>
#include <prs/production_rule.h>
#include <sch/Netlist.h>
#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Library.h>

#include <interpret_chp/import.h>
#include <interpret_hse/import.h>
#include <interpret_prs/import.h>
#include <interpret_sch/import.h>
#include <interpret_phy/import.h>

parse_ucs::source wvParse(weaver::Binder &bd, string path, string root) {
	size_t dot = path.find_last_of(".");
	size_t slash = path.find_last_of("/");
	if (slash == string::npos) {
		slash = 0u;
	} else {
		slash += 1;
	}

	string format = "";
	if (dot != string::npos) {
		format = path.substr(dot+1);
	} else {
		dot = path.size();
	}

	if (format != "wv") {
		printf("error: unrecognized source format '%s'\n", format.c_str());
	}

	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_ucs::source::register_syntax(tokens);
	bd.readPath(tokens, path, root);

	tokens.increment(true);
	tokens.expect<parse_ucs::source>();
	parse_ucs::source result;
	result.name = path.substr(slash, dot-slash);
	if (tokens.decrement(__FILE__, __LINE__)) {
		result.parse(tokens);
	}
	return result;
}

void wvFactory(weaver::Binder &bd, string path) {
	// TODO(edward.bingham) Implement modules appropriately:
	// 1. load top.wv from current directory
	// 2. import paths are relative to project root, so find project root

	// TODO(edward.bingham) find project root and load all files

	string root = bd.findProjectRoot(path);
	int offset = (int)bd.prgm.mods.size();

	vector<parse_ucs::source> src;
	vector<string> stack;
	stack.push_back(path);
	for (int i = 0; i < (int)stack.size(); i++) {
		src.push_back(wvParse(bd, stack[i], root));

		for (auto j = src.back().incl.begin(); j != src.back().incl.end(); j++) {
			for (auto k = j->path.begin(); k != j->path.end(); k++) {
				if (find(stack.begin(), stack.end(), k->second) == stack.end()) {
					string modPath = k->second.substr(1, k->second.size()-2)+".wv";
					stack.push_back(modPath);
				}
			}
		}
	}

	// load symbols to break dependency chains
	bd.prgm.mods.resize(offset+src.size());
	for (int i = 0; i < (int)src.size(); i++) {
		bd.loadSymbols(i+offset, src[i]);
	}

	// link up all of the dependencies
	for (int i = 0; i < (int)src.size(); i++) {
		bd.loadModule(i+offset, src[i]);
	}
}

string nameFromPath(string path) {
	// Remove directory part
	size_t slash = path.find_last_of("/\\");
	std::string filename = (slash == std::string::npos) ? path : path.substr(slash + 1);

	// Remove extension part
	size_t dot = filename.find_last_of('.');
	if (dot != std::string::npos) {
		filename = filename.substr(0, dot);
	}
	return filename;
}

void cogFactory(weaver::Binder &bd, string path) {
	string root = bd.findWorkingDir();

	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_cog::register_syntax(tokens);
	bd.readPath(tokens, path, root);

	chp::graph g;

	tokens.increment(false);
	parse_cog::expect(tokens);
	while (tokens.decrement(__FILE__, __LINE__)) {
		parse_cog::composition syntax(tokens);
		chp::import_chp(g, syntax, &tokens, true);

		tokens.increment(false);
		parse_cog::expect(tokens);
	}

	int kind = weaver::Term::findDialect("func");
	int modIdx = bd.prgm.createModule("__other__");

	int termIdx = bd.prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, nameFromPath(path), vector<weaver::Instance>()));

	bd.prgm.mods[modIdx].terms[termIdx].def = g;
}

void cogwFactory(weaver::Binder &bd, string path) {
	string root = bd.findWorkingDir();

	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_cog::register_syntax(tokens);
	bd.readPath(tokens, path, root);

	hse::graph g;

	tokens.increment(false);
	parse_cog::expect(tokens);
	while (tokens.decrement(__FILE__, __LINE__)) {
		parse_cog::composition syntax(tokens);
		hse::import_hse(g, syntax, &tokens, true);
		
		tokens.increment(false);
		parse_cog::expect(tokens);
	}

	g.post_process(true);
	g.check_variables();

	int kind = weaver::Term::findDialect("proto");
	int modIdx = bd.prgm.createModule("__other__");

	int termIdx = bd.prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, nameFromPath(path), vector<weaver::Instance>()));

	bd.prgm.mods[modIdx].terms[termIdx].def = g;
}

void prsFactory(weaver::Binder &bd, string path) {
	string root = bd.findWorkingDir();

	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_prs::register_syntax(tokens);
	bd.readPath(tokens, path, root);

	prs::production_rule_set pr;

	tokens.increment(false);
	parse_prs::expect(tokens);
	while (tokens.decrement(__FILE__, __LINE__)) {
		parse_prs::production_rule_set syntax(tokens);
		prs::import_production_rule_set(syntax, pr, -1, -1, prs::attributes(), 0, &tokens, true);
		
		tokens.increment(false);
		parse_prs::expect(tokens);
	}

	int kind = weaver::Term::findDialect("ckt");
	int modIdx = bd.prgm.createModule("__other__");

	int termIdx = bd.prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, nameFromPath(path), vector<weaver::Instance>()));

	bd.prgm.mods[modIdx].terms[termIdx].def = pr;
}

void spiFactory(weaver::Binder &bd, string path) {
	/*string root = bd.findWorkingDir();

	if (bd.tech.path.empty() and not phy::loadTech(bd.tech, bd.techPath, bd.cellsDir)) {
		cout << "Unable to load techfile \'" + bd.techPath + "\'." << endl;
		return false;
	}

	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_spice::register_syntax(tokens);
	bd.readPath(tokens, path, root);

	sch::Netlist net;

	tokens.increment(false);
	parse_spice::expect(tokens);
	while (tokens.decrement(__FILE__, __LINE__)) {
		parse_spice::netlist syntax(tokens);
		sch::import_netlist(tech, net, syntax, &tokens);

		tokens.increment(false);
		parse_spice::expect(tokens);
	}

	int kind = weaver::Term::findDialect("ckt");
	int modIdx = bd.prgm.createModule("__other__");

	int termIdx = bd.prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, nameFromPath(path), vector<weaver::Instance>()));

	bd.prgm.mods[modIdx].terms[termIdx].def = net;*/
}

void gdsFactory(weaver::Binder &bd, string path) {
}
