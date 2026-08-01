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
#include <interpret_chp/export.h>
#include <interpret_hse/export.h>

void readAstg(weaver::Project &proj, weaver::Source &source, string buffer) {
	parse_astg::graph::register_syntax(*source.tokens);
	source.tokens->insert(source.path.string(), buffer, nullptr);

	source.tokens->increment(false);
	source.tokens->expect<parse_astg::graph>();
	if (source.tokens->decrement(__FILE__, __LINE__)) {
		source.syntax = shared_ptr<parse::syntax>(new parse_astg::graph(*source.tokens));
	}
}

void loadAstg(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	chp::graph g;
	g.name = name;
	g = chp::import_chp(*(parse_astg::graph*)source.syntax.get(), source.tokens.get());

	weaver::Prototype proto = weaver::Prototype::fromMangled(name);

	weaver::TermId id;
	id.mod   = prgm.getModule(source.modName);
	id.index = prgm.modAt(id).createTerm(weaver::Term(proto.name, vector<weaver::Instance>()));
	id.var   = prgm.termAt(id).createVariant(weaver::Variant("func", g));
}

void loadAstgw(weaver::Project &proj, weaver::Program &prgm, const weaver::Source &source) {
	string name = source.path.stem().string();
	hse::graph g;
	g.name = name;
	hse::import_hse(g, *(parse_astg::graph*)source.syntax.get(), source.tokens.get());

	g.post_process(true, false, false, false);
	g.check_variables();

	weaver::Prototype proto = weaver::Prototype::fromMangled(name);

	weaver::TermId id;
	id.mod   = prgm.getModule(source.modName);
	id.index = prgm.modAt(id).createTerm(weaver::Term(proto.name, vector<weaver::Instance>()));
	id.var   = prgm.termAt(id).createVariant(weaver::Variant("proto", g));
}

void writeAstg(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id) {
	string pathstr = path.string();
	ofstream fout(pathstr.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", pathstr.c_str());
		return;
	}

	const chp::graph &g = prgm.varAt(id).as<chp::graph>();
	string buffer = parse_astg::export_astg(g).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}

void writeAstgw(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id) {
	string pathstr = path.string();
	ofstream fout(pathstr.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", pathstr.c_str());
		return;
	}

	const hse::graph &g = prgm.varAt(id).as<hse::graph>();
	string buffer = parse_astg::export_astg(g).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}
