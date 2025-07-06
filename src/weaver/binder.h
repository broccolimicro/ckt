#pragma once

#include "symbol.h"

#include <parse/tokenizer.h>
#include <parse_ucs/source.h>

#include <vector>
#include <string>

using std::vector;
using std::string;

namespace weaver {

struct Binder {
	Binder();
	~Binder();

	//vector<string> includePath;

	string findWorkingDir() const;
	string findProjectRoot(string workingDir) const;

	void readPath(tokenizer &tokens, string path, string root="");
	parse_ucs::source parsePath(string path, string root="");
	//void addIncludePath(string path);

	void loadSymbols(Program &prgm, int index, const parse_ucs::source &syntax);
	void loadModule(Program &prgm, int index, const parse_ucs::source &syntax);
	void load(Program &prgm, string path);
};

}
