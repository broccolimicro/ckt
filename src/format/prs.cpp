#include "prs.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_prs/factory.h>
#include <prs/production_rule.h>

#include <interpret_prs/import.h>
#include <interpret_prs/export.h>

void readPrs(choy::Project &proj, choy::Source &source, string buffer) {
	source.tokens->register_token<parse::block_comment>(false);
	source.tokens->register_token<parse::line_comment>(false);
	parse_prs::production_rule_set::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);
	
	source.tokens->increment(false);
	source.tokens->expect<parse_prs::production_rule_set>();
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_prs::production_rule_set(*source.tokens));
	}
}

std::vector<choy::TermId> loadPrs(choy::Project &proj, choy::Program &prgm, const choy::Source &source) {
	string name = source.path.stem().string();
	prs::production_rule_set pr;
	pr.name = name;
	prs::import_production_rule_set(*(parse_prs::production_rule_set*)source.syntax.get(), pr, -1, -1, prs::attributes(), 0, source.tokens.get(), true);

	choy::Prototype proto = choy::Prototype::fromMangled(name, source.modName);
	int mod = prgm.getModule(proto.mod);

	choy::TermId id = prgm.getTerm(proto, mod);
	id.var = prgm.termAt(id).createVariant(choy::Variant("circ", pr));
	prgm.varAt(id).fromSource = true;
	return {id};
}

void writePrs(fs::path path, choy::Project &proj, const choy::Filetype &lang, const choy::Program &prgm, choy::TermId id) {
	string pathstr = path.string();
	ofstream fout(pathstr.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", pathstr.c_str());
		return;
	}

	const prs::production_rule_set &pr = prgm.varAt(id).as<prs::production_rule_set>();
	string buffer = parse_prs::export_production_rule_set(pr).to_string();
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
