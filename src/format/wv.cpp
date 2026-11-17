#include "choy.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_choy/source.h>

#include <interpret_choy/import.h>

void readWv(choy::Project &proj, choy::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_choy::source::register_syntax(*source.tokens, &proj);
	source.tokens->insert(source.path.string(), buffer, &proj);

	source.tokens->increment(true);
	source.tokens->expect<parse_choy::source>((const parse::registry*)&proj);
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_choy::source(*source.tokens, (const parse::registry*)&proj));
	}

	parse_choy::source &syntax = *(parse_choy::source*)source.syntax.get();
	syntax.name = source.modName;

	for (auto j = syntax.incl.begin(); j != syntax.incl.end(); j++) {
		for (auto k = j->path.begin(); k != j->path.end(); k++) {
			string modPath = k->second.substr(1, k->second.size()-2);
			proj.incl(modPath);
		}
	}
}

std::vector<choy::TermId> loadWv(choy::Project &proj, choy::Program &prgm, const choy::Source &source) {
	int index = prgm.getModule(source.modName);
	// load symbols to break dependency chains
	import_symbols(prgm, index, *(parse_choy::source*)source.syntax.get(), source.tokens.get());
	// link up all of the dependencies
	return import_module(proj, prgm, index, *(parse_choy::source*)source.syntax.get(), source.tokens.get());
}
