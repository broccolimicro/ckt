#pragma once

#include <vector>
#include <string>
#include <any>

#include <common/interface.h>
#include <parse/syntax.h>
#include <parse/tokenizer.h>

using std::vector;
using std::string;
using std::any;

namespace weaver {

// TODO(edward.bingham) Syntax directed translation from CHP to val-rdy
// dataflow blocks to map to FPGA Golang runtime with thread bool for parallel
// routines, compile to software MLIR? LLVM?
//
// LLVM represents programs as a combined control-flow data-flow graph with scope
// implemented by subgraphs I'm currently representing CHP as a petri-net. Primary
// difference is the flattening of scope.

using TypeID=array<int, 2>;

static const TypeID UNDEF={-1,-1};

// represents variable declarations, this does not include "connections" for functions
struct Instance {
	Instance();
	Instance(TypeID type, string name, vector<int> size=vector<int>());
	~Instance();
	
	TypeID type;
	string name;
	vector<int> size;

	void print();
};

bool operator==(const Instance &i0, const Instance &i1);

//     wires, data      devices, behaviors
// interface -> type, context -> func, struct

// represents function prototypes
struct Decl {
	Decl();
	Decl(string name, vector<Instance> args, TypeID ret=UNDEF, TypeID recv=UNDEF);
	~Decl();

	TypeID recv;
	string name;
	vector<Instance> args;
	TypeID ret;

	void print();
};

bool operator==(const Decl &d0, const Decl &d1);

struct Dialect {
	typedef std::any (*Factory)(const parse::syntax*, tokenizer*);

	Dialect();
	Dialect(string name, Factory factory);
	~Dialect();
	
	string name;
	Factory factory;
};

struct Scope {
	enum {
		NOTFOUND=-1
	};
	vector<Instance> tbl;
	int parent;
	vector<int> child;

	int find(string name) const;
};

struct SymbolTable {
	vector<Scope> scope;
	int curr;

	bool define(Instance inst);
	void pushScope();
	void popScope();
	
	Instance find(string name) const;
};

struct Term {
	Term();
	~Term();

	enum {
		NONE = -2,
		CONTEXT = -1,
		PROCESS = 0,
	};

	static vector<Dialect> dialects;

	int kind;
	Decl decl;
	SymbolTable symb;
	std::any def;

	static Term procOf(int kind, string name, vector<Instance> args, TypeID ret=UNDEF, TypeID recv=UNDEF);
	static Term contextOf(string name, vector<Instance> args, TypeID ret=UNDEF, TypeID recv=UNDEF);

	static int pushDialect(string name, Dialect::Factory factory);
	static int findDialect(string name);

	void print();
};

struct Type {
	Type();
	~Type();

	enum {
		NONE = -2,
		INTERFACE = -1,
		TYPE = 0,
	};

	int kind;
	string name;
	vector<Instance> members;
	vector<Decl> methods;

	static Type typeOf(string name, vector<Instance> members=vector<Instance>(), vector<Decl> methods=vector<Decl>());
	static Type interfaceOf(string name, vector<Decl> methods=vector<Decl>());

	void print();
};

struct Module {
	string name;
	vector<Term> terms;
	vector<Type> types;
	vector<pair<string, string> > aliases;

	int createTerm(Term term);
	int createType(Type type);

	int findTerm(Decl proto) const;
	int findType(vector<string> name) const;

	void print();
};

struct Program {
	Program();
	~Program();

	vector<Module> mods;
	int global;

	int createModule(string name);

	int findTerm(int index, Decl proto) const;
	TypeID findType(int index, vector<string> name) const;

	void print();
};

void loadGlobalTypes(Program &prgm);

// Variables in Weaver are compound (type, interface)
// Variables in flow are channels (type)
// Variables in chp are data (fixed, float, bool)
// Variables in hse and prs are wires (bool)

// How do I map variables in chp back to weaver?
// Weaver should have some structure to keep track of instances. but does scope matter? Scope only matters to avoid overlap of variable names, so only matters during interpretation.


/*Instance at(Module &types, SymbolTable &symb, vector<int> idx);

struct Scope {
	vector<SymbolTable> scope;

	array<int, 2> find(string name) const;
};*/

}
