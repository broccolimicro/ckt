#include "verilog.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <interpret_flow/export_verilog.h>

void writeVerilog(fs::path path, weaver::Project &proj, const weaver::Filetype &lang, const weaver::Program &prgm, weaver::TermId id) {
	string pathstr = path.string();
	ofstream fout(pathstr.c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", pathstr.c_str());
		return;
	}

	const clocked::Module &mod = prgm.varAt(id).as<clocked::Module>();
	string buffer = flow::export_module(mod).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}

