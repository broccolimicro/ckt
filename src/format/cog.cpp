#include "cog.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_cog/adapter.h>
#include <parse_chp/factory.h>
#include <chp/graph.h>
#include <hse/graph.h>

#include <parse_ucs/type_name.h>

#include <interpret_chp/import.h>
#include <interpret_hse/import.h>

void readCog(weaver::Project &proj, weaver::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_cog::composition::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);

	parse_cog::adapter cfg;
	cfg.type_name.set<parse_ucs::type_name>();

	source.tokens->increment(false);
	source.tokens->expect<parse_cog::composition>();
	if (source.tokens->decrement(__FILE__, __LINE__, &cfg)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_cog::composition(*source.tokens, 0, &cfg));
	}
}

void loadCog(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	chp::graph g = std::any_cast<chp::graph>(factoryCog(name, source.syntax.get(), source.tokens.get()));

	weaver::Prototype proto = weaver::Prototype::fromMangled(name);
	proto.mod = source.modName;

	weaver::TermId id;
	id.mod   = prgm.getModule(source.modName);
	id.index = prgm.modAt(id).createTerm(weaver::Term(proto.name, vector<weaver::Instance>()));
	id.var   = prgm.termAt(id).createVariant(weaver::Variant("func", g));
}

void loadCogw(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	hse::graph g = std::any_cast<hse::graph>(factoryCogw(name, source.syntax.get(), source.tokens.get()));

	weaver::Prototype proto = weaver::Prototype::fromMangled(name);
	proto.mod = source.modName;

	weaver::TermId id;
	id.mod   = prgm.getModule(proto.mod);
	id.index = prgm.modAt(id).createTerm(weaver::Term(proto.name, vector<weaver::Instance>()));
	id.var   = prgm.termAt(id).createVariant(weaver::Variant("proto", g));
}

std::any factoryCog(string name, const parse::syntax *syntax, tokenizer *tokens) {
	chp::graph g;
	g.name = name;
	if (syntax != nullptr) {
		chp::import_chp(g, *(const parse_cog::composition *)syntax, tokens, true);
		g.post_process(true);
	}
	return g;
}

std::any factoryCogw(string name, const parse::syntax *syntax, tokenizer *tokens) {
	hse::graph g;
	g.name = name;
	if (syntax != nullptr) {
		hse::import_hse(g, *(const parse_cog::composition *)syntax, tokens, true);
		g.post_process(true);
		g.check_variables();
	}
	return g;
}
