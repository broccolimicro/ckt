#include "verilog.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <interpret_flow/export_verilog.h>

void writeVerilog(fs::path path, const weaver::Project &proj, const weaver::Program &prgm, int modIdx, int termIdx) {
	ofstream fout(path.string().c_str(), ios::out);
	if (not fout.is_open()) {
		printf("error: unable to write to file '%s'\n", path.string().c_str());
		return;
	}

	const clocked::Module &mod = prgm.mods[modIdx].terms[termIdx].as<clocked::Module>();
	string buffer = flow::export_module(mod).to_string();
	fout.write(buffer.c_str(), buffer.size());
	fout.close();
}

