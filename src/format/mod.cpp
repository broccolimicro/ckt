#include "mod.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/modfile.h>

#include <interpret_wv/import.h>
#include <interpret_wv/export.h>

void readMod(weaver::Project &proj) {
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_ucs::modfile::register_syntax(tokens);

	ifstream fin;
	string pathstr = (proj.rootDir / "lm.mod").string();
	fin.open(pathstr.c_str(), ios::binary | ios::in);
	if (!fin.is_open()) {
		tokens.error("file not found '" + (proj.rootDir / "lm.mod").string() + "'", __FILE__, __LINE__);
	} else {
		fin.seekg(0, ios::end);
		int size = (int)fin.tellg();
		string buffer(size, ' ');
		fin.seekg(0, ios::beg);
		fin.read(&buffer[0], size);
		fin.clear();
		tokens.insert(pathstr, buffer, nullptr);
	}

	tokens.increment(true);
	tokens.expect<parse_ucs::modfile>();

	if (tokens.decrement(__FILE__, __LINE__)) {
		parse_ucs::modfile syntax(tokens);

		import_modfile(proj, syntax, &tokens);
	}
}

void writeMod(const weaver::Project &proj) {
	parse_ucs::modfile result = parse_ucs::export_modfile(proj);

	ofstream fout;
	string pathstr = (proj.rootDir / "lm.mod").string();
	fout.open(pathstr.c_str(), ios::out);
	string buf = result.to_string("");
	fout.write(buf.c_str(), buf.size());
	fout.close();
}
