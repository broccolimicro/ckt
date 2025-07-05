#include "symbol.h"

#include <chp/graph.h>
#include "../format/dot.h"
#include <interpret_chp/export.h>

namespace weaver {

vector<Dialect> Term::dialects;

Instance::Instance() {
}

Instance::Instance(TypeID type, string name, vector<int> size) {
	this->type = type;
	this->name = name;
	this->size = size;
}

Instance::~Instance() {
}

void Instance::print() {
	printf("instance (%d,%d) %s {", type[0], type[1], name.c_str());
	for (int i = 0; i < (int)size.size(); i++) {
		printf("%d ", size[i]);
	}
	printf("}\n");
}

bool operator==(const Instance &i0, const Instance &i1) {
	if (i0.type != i1.type or i0.size.size() != i1.size.size()) {
		return false;
	}
	
	for (int i = 0; i < (int)i0.size.size(); i++) {
		if (i0.size[i] != i1.size[i]) {
			return false;
		}
	}
	
	return true;
}

Decl::Decl() {
}

Decl::Decl(string name, vector<Instance> args, TypeID ret, TypeID recv) {
 	this->name = name;
	this->args = args;
	this->ret = ret;
	this->recv = recv;
}

Decl::~Decl() {
}

void Decl::print() {
	printf("decl (%d,%d) %s {\n", recv[0], recv[1], name.c_str());
	for (int i = 0; i < (int)args.size(); i++) {
		args[i].print();
	}
	printf("} (%d,%d)\n", ret[0], ret[1]);
}

bool operator==(const Decl &d0, const Decl &d1) {
	if (d0.recv != d1.recv or d0.name != d1.name or d0.ret != d1.ret or d0.args.size() != d1.args.size()) {
		return false;
	}

	for (int i = 0; i < (int)d0.args.size(); i++) {
		if (not (d0.args[i] == d1.args[i])) {
			return false;
		}
	}

	return true;
}	

Dialect::Dialect() {
	name = "";
	factory = nullptr;
}

Dialect::Dialect(string name, Dialect::Factory factory) {
	this->name = name;
	this->factory = factory;
}

Dialect::~Dialect() {
}

int Scope::find(string name) const {
	for (int i = 0; i < (int)tbl.size(); i++) {
		if (tbl[i].name == name) {
			return i;
		}
	}
	return NOTFOUND;
}

bool SymbolTable::define(Instance inst) {
	if (scope[curr].find(inst.name) != Scope::NOTFOUND) {
		return false;
	}

	scope[curr].tbl.push_back(inst);
	return true;
}

void SymbolTable::pushScope() {
	scope.push_back(Scope());
	scope[curr].child.push_back((int)scope.size()-1);
	scope.back().parent = curr;
	curr = (int)scope.size()-1;
}

void SymbolTable::popScope() {
	curr = scope[curr].parent;
}

Term::Term() {
}

Term::~Term() {
}

Term Term::procOf(int kind, string name, vector<Instance> args, TypeID ret, TypeID recv) {
	Term result;
	result.kind = kind;
	result.decl = Decl(name, args, ret, recv);
	return result;
}

Term Term::contextOf(string name, vector<Instance> args, TypeID ret, TypeID recv) {
	Term result;
	result.kind = Term::CONTEXT;
	result.decl = Decl(name, args, ret, recv);
	return result;
}

int Term::pushDialect(string name, Dialect::Factory factory) {
	dialects.push_back(Dialect(name, factory));
	return (int)dialects.size()-1;
}

int Term::findDialect(string name) {
	for (int i = 0; i < (int)dialects.size(); i++) {
		if (dialects[i].name == name) {
			return i;
		}
	}
	return Term::NONE;
}

void Term::print() {
	printf("term %d ", kind);
	decl.print();
	if (kind == 0) {
		gvdot::render("test.png", export_graph(*std::any_cast<chp::graph>(&def), false).to_string());
	}
}

Type::Type() {
}

Type::~Type() {
}

Type Type::typeOf(string name, vector<Instance> members, vector<Decl> methods) {
	Type result;
	result.kind = TYPE;
	result.name = name;
	result.members = members;
	result.methods = methods;
	return result;
}

Type Type::interfaceOf(string name, vector<Decl> methods) {
	Type result;
	result.kind = INTERFACE;
	result.name = name;
	result.methods = methods;
	return result;
}

void Type::print() {
	printf("type %d %s {\n", kind, name.c_str());
	for (int i = 0; i < (int)members.size(); i++) {
		members[i].print();
	}
	for (int i = 0; i < (int)methods.size(); i++) {
		methods[i].print();
	}
	printf("}\n");
}

int Module::createTerm(Term term) {
	terms.push_back(term);
	return (int)terms.size()-1;
}

int Module::createType(Type type) {
	types.push_back(type);
	return (int)types.size()-1;
}

int Module::findTerm(Decl proto) const {
	for (int i = 0; i < (int)terms.size(); i++) {
		if (terms[i].decl == proto) {
			return i;
		}
	}
	return -1;
}

int Module::findType(vector<string> name) const {
	if (name.size() == 1u) {
		for (int i = 0; i < (int)types.size(); i++) {
			if (types[i].name == name[0]) {
				return i;
			}
		}
	} else {
		printf("error: unimplemented\n");
		// function call?
	}
	return -1;
}

void Module::print() {
	printf("module %s {\n", name.c_str());
	for (int i = 0; i < (int)types.size(); i++) {
		types[i].print();
	}
	for (int i = 0; i < (int)terms.size(); i++) {
		terms[i].print();
	}
	printf("}\n");
}

Instance SymbolTable::find(string name) const {
	int i = curr;
	int index;
	do {
		index = scope[i].find(name);
	} while (index == Scope::NOTFOUND and i >= 0);

	if (i >= 0 and index >= 0) {
		return scope[i].tbl[index];
	}
	return Instance();
}

Program::Program() {
	global = -1;
}

Program::~Program() {
}

int Program::createModule(string name) {
	mods.push_back(Module());
	mods.back().name = name;
	return (int)mods.size()-1;
}

int Program::findTerm(int index, Decl proto) const {
	size_t dot = proto.name.find_first_of(".");
	if (dot == string::npos) {
		return -1;
	}

	string modName = proto.name.substr(0, dot);
	proto.name = proto.name.substr(dot+1);
	for (int i = 0; i < (int)mods.size(); i++) {
		if (mods[i].name == modName) {
			return mods[i].findTerm(proto);
		}
	}
	return -1;
}

TypeID Program::findType(int index, vector<string> name) const {
	if (name.empty()) {
		return UNDEF;
	} else if (name.size() == 1u) {
		TypeID result = UNDEF;
		if (global >= 0) {
			result[0] = global;
			result[1] = mods[global].findType(name);
		}

		if (result[1] == UNDEF[1] and index >= 0) {
			result[0] = index;
			result[1] = mods[index].findType(name);
		}
		
		return result;
	}

	string modName = name[0];
	name.erase(name.begin());
	for (int i = 0; i < (int)mods.size(); i++) {
		if (mods[i].name == modName) {
			return {i, mods[i].findType(name)};
		}
	}
	return UNDEF;
}

void Program::print() {
	for (int i = 0; i < (int)mods.size(); i++) {
		mods[i].print();
	}
}

void loadGlobalTypes(Program &prgm) {
	if (prgm.global >= 0) {
		return;
	}
	Module glob;
	glob.types.push_back(Type::interfaceOf("chan"));
	glob.types.push_back(Type::interfaceOf("fixed"));
	glob.types.push_back(Type::interfaceOf("ufixed"));
	glob.types.push_back(Type::interfaceOf("bool"));
	prgm.mods.push_back(glob);
	prgm.global = (int)prgm.mods.size()-1;
}

/*Instance at(Module &types, SymbolTable &symb, vector<int> idx) {
	
}*/

}
