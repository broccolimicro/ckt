#include "wv.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>

#include "../weaver/import.h"

void readWv(weaver::Project &proj, weaver::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_ucs::source::register_syntax(*source.tokens);
	source.tokens->insert(source.path, buffer, nullptr);

	source.tokens->increment(true);
	source.tokens->expect<parse_ucs::source>();
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = unique_ptr<parse::syntax>(new parse_ucs::source(*source.tokens));
	}

	parse_ucs::source &syntax = *(parse_ucs::source*)source.syntax.get();
	syntax.name = source.path.stem().string();

	for (auto j = syntax.incl.begin(); j != syntax.incl.end(); j++) {
		for (auto k = j->path.begin(); k != j->path.end(); k++) {
			string modPath = k->second.substr(1, k->second.size()-2)+".wv";
			proj.incl(modPath, source.path.parent_path());
		}
	}
}

void loadWv(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	int index = (int)prgm.mods.size();
	prgm.mods.push_back(weaver::Module());
	// load symbols to break dependency chains
	import_symbols(prgm, index, *(parse_ucs::source*)source.syntax.get());
	// link up all of the dependencies
	import_module(prgm, index, *(parse_ucs::source*)source.syntax.get());
}
