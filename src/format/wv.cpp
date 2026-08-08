#include "wv.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>

#include <interpret_wv/import.h>

void readWv(weaver::Project &proj, weaver::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_ucs::source::register_syntax(*source.tokens, &proj);
	source.tokens->insert(source.path.string(), buffer, &proj);

	source.tokens->increment(true);
	source.tokens->expect<parse_ucs::source>((const parse::registry*)&proj);
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_ucs::source(*source.tokens, (const parse::registry*)&proj));
	}

	parse_ucs::source &syntax = *(parse_ucs::source*)source.syntax.get();
	syntax.name = source.modName;

	for (auto j = syntax.incl.begin(); j != syntax.incl.end(); j++) {
		for (auto k = j->path.begin(); k != j->path.end(); k++) {
			string modPath = k->second.substr(1, k->second.size()-2);
			proj.incl(modPath);
		}
	}
}

std::vector<weaver::TermId> loadWv(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	int index = prgm.getModule(source.modName);
	// load symbols to break dependency chains
	import_symbols(prgm, index, *(parse_ucs::source*)source.syntax.get(), source.tokens.get());
	// link up all of the dependencies
	return import_module(proj, prgm, index, *(parse_ucs::source*)source.syntax.get(), source.tokens.get());
}
