#include "cog.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <chp/graph.h>
#include <hse/graph.h>

#include <interpret_chp/import.h>
#include <interpret_hse/import.h>
#include <interpret_chp/export.h>
#include <interpret_hse/export.h>

void readAstg(weaver::Project &proj, weaver::Source &source, string buffer) {
	parse_astg::graph::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);

	source.tokens->increment(false);
	source.tokens->expect<parse_astg::graph>();
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_astg::graph(*source.tokens));
	}
}

void loadAstg(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	chp::graph g;
	g.name = name;
	g = chp::import_chp(*(parse_astg::graph*)source.syntax.get(), source.tokens.get());

	int kind = weaver::Term::getDialect("func");
	int modIdx = prgm.getModule(source.modName);

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, name, vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = g;
}

void loadAstgw(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	hse::graph g;
	g.name = name;
	hse::import_hse(g, *(parse_astg::graph*)source.syntax.get(), source.tokens.get());

	g.post_process(true);
	g.check_variables();

	int kind = weaver::Term::getDialect("proto");
	int modIdx = prgm.getModule(source.modName);

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, name, vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = g;
}

void writeAstg(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	ofstream fout(path.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", path.c_str());
		return;
	}

	const chp::graph &g = prgm.mods[modIdx].terms[termIdx].as<chp::graph>();
	string buffer = chp::export_astg(g).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}

void writeAstgw(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	ofstream fout(path.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", path.c_str());
		return;
	}

	const hse::graph &g = prgm.mods[modIdx].terms[termIdx].as<hse::graph>();
	string buffer = hse::export_astg(g).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}
