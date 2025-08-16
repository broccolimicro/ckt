#include "dialect.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

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


