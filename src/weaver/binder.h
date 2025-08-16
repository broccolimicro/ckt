#pragma once

#include <common/net.h>

#include <parse/tokenizer.h>
#include <parse_ucs/source.h>

#include <vector>
#include <string>

#include <weaver/program.h>

using std::vector;
using std::string;

namespace weaver {

struct Binder {
	Binder(Program &prgm);
	~Binder();

	Program &prgm;
	int currModule;
	int currTerm;

	enum {
		NONE = -1,
	};

	struct Dialect {
		typedef void (*Factory)(Binder&, string);

		Dialect();
		Dialect(string name, Factory factory);
		~Dialect();

		string name;
		Factory factory;
	};

	static vector<Dialect> dialects;

	// Registers a new dialect with a name and factory function
	// Returns the index of the newly registered dialect
	static int pushDialect(string name, Dialect::Factory factory);
	
	// Finds a dialect by name
	// Returns the index of the dialect, or NONE if not found
	static int findDialect(string name);

	void readPath(tokenizer &tokens, string path, string root);

	// Managing the project
	string findWorkingDir() const;
	string findProjectRoot(string workingDir="") const;

	// Managing scope
	bool define(vector<string> typeName, string name, vector<int> size, ucs::Netlist nets);
	void pushScope();
	void popScope();

	// Loading the program
	void loadSymbols(int index, const parse_ucs::source &syntax);
	void loadModule(int index, const parse_ucs::source &syntax);
	void load(string path="");
};

}
