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

void readCog(weaver::Project &proj, weaver::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_cog::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);

	source.tokens->increment(false);
	parse_cog::expect(*source.tokens);
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = unique_ptr<parse::syntax>(new parse_cog::composition(*source.tokens));
	}
}

void loadCog(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	chp::graph g;
	chp::import_chp(g, *(parse_cog::composition*)source.syntax.get(), source.tokens.get(), true);

	int kind = weaver::Term::getDialect("func");
	int modIdx = prgm.createModule("__other__");

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, source.path.stem().string(), vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = g;
}

void loadCogw(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	hse::graph g;
	hse::import_hse(g, *(parse_cog::composition*)source.syntax.get(), source.tokens.get(), true);

	g.post_process(true);
	g.check_variables();

	int kind = weaver::Term::getDialect("proto");
	int modIdx = prgm.createModule("__other__");

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, source.path.stem().string(), vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = g;
}

std::any factoryCog(const parse::syntax *syntax, tokenizer *tokens) {
	chp::graph g;
	if (syntax != nullptr) {
		chp::import_chp(g, *(const parse_cog::composition *)syntax, tokens, true);
		//g.post_process(true);
	}
	return g;
}

std::any factoryCogw(const parse::syntax *syntax, tokenizer *tokens) {
	hse::graph g;
	if (syntax != nullptr) {
		hse::import_hse(g, *(const parse_cog::composition *)syntax, tokens, true);
		g.post_process(true);
		g.check_variables();
	}
	return g;
}
