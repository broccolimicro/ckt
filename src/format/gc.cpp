#include "gc.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_gc/factory.h>
#include <parse_gc/rule_set.h>
#include <gc/guarded_command.h>

#include <interpret_gc/import.h>
#include <interpret_gc/export.h>

void readGc(weaver::Project &proj, weaver::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_gc::register_syntax(*source.tokens);
	source.tokens->insert(source.path, buffer, nullptr);
	
	source.tokens->increment(false);
	parse_gc::expect(*source.tokens);
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_gc::rule_set(*source.tokens));
	}
}

void loadGc(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	gc::GuardedCommands rules;
	rules.name = name;
	gc::import_rule_set(*(parse_gc::rule_set*)source.syntax.get(), rules, 0, source.tokens.get(), true);

	weaver::Prototype proto = prgm.parseMangledName(name);

	weaver::TermId id;
	id.mod   = prgm.getModule(source.modName);
	id.index = prgm.modAt(id).createTerm(weaver::Term(proto.name, vector<weaver::Instance>()));
	id.var   = prgm.termAt(id).createVariant(weaver::Variant("struct", rules));
}

void writeGc(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, int modIdx, int termIdx, int varIdx) {
	string pathstr = path.string();
	ofstream fout(pathstr.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", pathstr.c_str());
		return;
	}

	const gc::GuardedCommands &rules = prgm.mods[modIdx].terms[termIdx].variants[varIdx].as<gc::GuardedCommands>();
	string buffer = gc::export_rule_set(rules).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}

std::any factoryGc(string name, const parse::syntax *syntax, tokenizer *tokens) {
	gc::GuardedCommands rules;
	rules.name = name;
	if (syntax != nullptr) {
		gc::import_rule_set(*(const parse_gc::rule_set *)syntax, rules, 0, tokens, true);
	}
	return rules;
}
