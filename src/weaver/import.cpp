#include "import.h"

#include <filesystem>

namespace weaver {

/*bool Binder::define(vector<string> typeName, string name, vector<int> size, ucs::Netlist nets) {
	TypeId type = prgm.findType(currModule, typeName);
	Instance newInst(type, name, size);
	if (prgm.mods[currModule].terms[currTerm].symb.define(newInst)) {
		vector<int> i;
		i.resize(size.size(), 0);
		return true;
	}
	return false;
}

void Binder::pushScope() {
	prgm.mods[currModule].terms[currTerm].symb.pushScope();
}

void Binder::popScope() {
	prgm.mods[currModule].terms[currTerm].symb.popScope();
}*/

bool import_declaration(vector<Instance> &result, const Program &prgm, int modIdx, const parse_ucs::declaration &syntax) {
	TypeId type = prgm.findType(modIdx, syntax.type.names);
	if (not type.defined()) {
		printf("error: type not defined '%s'\n", syntax.type.to_string().c_str());
		return false;
	}

	for (int k = 0; k < (int)syntax.name.size(); k++) {
		result.push_back(Instance(type, syntax.name[k].name));
		// TODO(edward.bingham) reset behavior? arrays?
	}
	return true;
}

Decl import_prototype(const Program &prgm, int modIdx, const parse_ucs::prototype &syntax, TypeId recvType) {
	TypeId retType = prgm.findType(modIdx, syntax.ret.names);
	vector<Instance> args;
	for (auto i = syntax.args.begin(); i != syntax.args.end(); i++) {
		import_declaration(args, prgm, modIdx, *i);
	}

	return Decl(syntax.name, args, retType, recvType);
}

void import_symbols(Program &prgm, int modIdx, const parse_ucs::source &syntax) {
	for (auto i = syntax.types.begin(); i != syntax.types.end(); i++) {
		int recvType = prgm.mods[modIdx].createType(Type::typeOf(i->name));
	}
}

void import_module(Program &prgm, int modIdx, const parse_ucs::source &syntax) {
	for (auto i = syntax.types.begin(); i != syntax.types.end(); i++) {
		TypeId recvType = prgm.findType(modIdx, {i->name});
		for (auto j = i->members.begin(); j != i->members.end(); j++) {
			import_declaration(prgm.typeAt(recvType).members, prgm, modIdx, *j);
		}

		for (auto j = i->protocols.begin(); j != i->protocols.end(); j++) {
			prgm.typeAt(recvType).methods.push_back(import_prototype(prgm, modIdx, *j, recvType));
		}
	}

	for (auto i = syntax.funcs.begin(); i != syntax.funcs.end(); i++) {
		int kind = Term::findDialect(i->lang);
		TypeId recvType;
		if (not i->recv.empty()) {
			recvType = prgm.findType(modIdx, {i->recv});
			if (not recvType.defined()) {
				printf("error: type not defined '%s'\n", i->recv.c_str());
				continue;
			}
		}

		TypeId retType;
		if (i->ret.valid) {
			retType = prgm.findType(modIdx, i->ret.names);
			if (not retType.defined()) {
				printf("error: type not defined '%s'\n", i->ret.to_string().c_str());
				continue;
			}
		}

		vector<Instance> args;
		for (auto j = i->args.begin(); j != i->args.end(); j++) {
			import_declaration(args, prgm, modIdx, *j);
		}

		int termIdx = prgm.mods[modIdx].createTerm(Term::procOf(kind, i->name, args, retType, recvType));

		if (kind >= 0) {
			prgm.mods[modIdx].terms[termIdx].def = Term::dialects[kind].factory(i->body, nullptr);
		}
	}
}

}
