#pragma once

#include "symbol.h"

#include <common/net.h>

#include <parse/tokenizer.h>
#include <parse_ucs/source.h>

#include <vector>
#include <string>

using std::vector;
using std::string;

namespace weaver {

struct Binder {
	Binder(Program &prgm);
	~Binder();

	Program &prgm;
	int currModule;
	int currTerm;

	// Managing the project
	string findWorkingDir() const;
	string findProjectRoot(string workingDir) const;

	// Reading source files
	void readPath(tokenizer &tokens, string path, string root="");
	parse_ucs::source parsePath(string path, string root="");

	// Managing scope
	bool define(vector<string> typeName, string name, vector<int> size, ucs::Netlist nets);
	void pushScope();
	void popScope();

	// Loading the program
	void loadSymbols(int index, const parse_ucs::source &syntax);
	void loadModule(int index, const parse_ucs::source &syntax);
	void load(string path);
};

}
