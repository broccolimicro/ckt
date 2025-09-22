#include "prs.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_prs/factory.h>
#include <prs/production_rule.h>

#include <interpret_prs/import.h>
#include <interpret_prs/export.h>

void readPrs(weaver::Project &proj, weaver::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_prs::register_syntax(*source.tokens);
	source.tokens->insert(source.path, buffer, nullptr);
	
	source.tokens->increment(false);
	parse_prs::expect(*source.tokens);
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_prs::production_rule_set(*source.tokens));
	}
}

void loadPrs(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	prs::production_rule_set pr;
	pr.name = name;
	prs::import_production_rule_set(*(parse_prs::production_rule_set*)source.syntax.get(), pr, -1, -1, prs::attributes(), 0, source.tokens.get(), true);

	int kind = weaver::Term::getDialect("circ");
	int modIdx = prgm.getModule(source.modName);

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, name, vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = pr;
}

void writePrs(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	string pathstr = path.string();
	ofstream fout(pathstr.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", pathstr.c_str());
		return;
	}

	const prs::production_rule_set &pr = prgm.mods[modIdx].terms[termIdx].as<prs::production_rule_set>();
	string buffer = prs::export_production_rule_set(pr).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}

std::any factoryPrs(string name, const parse::syntax *syntax, tokenizer *tokens) {
	prs::production_rule_set pr;
	pr.name = name;
	if (syntax != nullptr) {
		prs::import_production_rule_set(*(const parse_prs::production_rule_set *)syntax, pr, -1, -1, prs::attributes(), 0, tokens, true);
	}
	return pr;
}
