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
		source.syntax = unique_ptr<parse::syntax>(new parse_prs::production_rule_set(*source.tokens));
	}
}

void loadPrs(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	prs::production_rule_set pr;
	prs::import_production_rule_set(*(parse_prs::production_rule_set*)source.syntax.get(), pr, -1, -1, prs::attributes(), 0, source.tokens.get(), true);

	int kind = weaver::Term::getDialect("ckt");
	int modIdx = prgm.getModule(source.modName.string());

	int termIdx = prgm.mods[modIdx].createTerm(weaver::Term::procOf(kind, source.modName.stem().string(), vector<weaver::Instance>()));

	prgm.mods[modIdx].terms[termIdx].def = pr;
}

void writePrs(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	ofstream fout(path.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", path.c_str());
		return;
	}

	const prs::production_rule_set &pr = std::any_cast<const prs::production_rule_set&>(prgm.mods[modIdx].terms[termIdx].def);
	string buffer = prs::export_production_rule_set(pr).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}

std::any factoryPrs(const parse::syntax *syntax, tokenizer *tokens) {
	prs::production_rule_set pr;
	if (syntax != nullptr) {
		prs::import_production_rule_set(*(const parse_prs::production_rule_set *)syntax, pr, -1, -1, prs::attributes(), 0, tokens, true);
	}
	return pr;
}
