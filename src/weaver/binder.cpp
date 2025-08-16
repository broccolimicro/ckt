#include "binder.h"

#include <filesystem>

namespace weaver {

vector<Binder::Dialect> Binder::dialects;

Binder::Dialect::Dialect() {
	factory = nullptr;
}

Binder::Dialect::Dialect(string name, Factory factory) {
	this->name = name;
	this->factory = factory;
}

Binder::Dialect::~Dialect() {
}

Binder::Binder(Program &prgm) : prgm(prgm) {
	loadGlobalTypes(this->prgm);
}

Binder::~Binder() {
}

int Binder::pushDialect(string name, Dialect::Factory factory) {
	// Register a new dialect with the given name and factory function
	dialects.push_back(Dialect(name, factory));
	// Return the index of the newly registered dialect
	return (int)dialects.size()-1;
}

int Binder::findDialect(string name) {
	// Search for a dialect with the given name
	for (int i = 0; i < (int)dialects.size(); i++) {
		if (dialects[i].name == name) {
			return i;
		}
	}
	// Return NONE if no matching dialect is found
	return Binder::NONE;
}

void Binder::readPath(tokenizer &tokens, string path, string root) {
	if (not root.empty() and path[0] != '/') {
		path = root + "/" + path;
	}

	if (tokens.segment_loading(path)) {
		tokens.error("cycle found in include graph", __FILE__, __LINE__);
	} else if (!tokens.segment_loaded(path)) {
		ifstream fin;
		fin.open(path.c_str(), ios::binary | ios::in);
		/*for (int i = 0; !fin.is_open() and i < (int)includePath.size(); i++) {
			fin.open((includePath[i] + path).c_str(), ios::binary | ios::in);
		}*/

		if (!fin.is_open()) {
			tokens.error("file not found '" + path + "'", __FILE__, __LINE__);
		} else {
			fin.seekg(0, ios::end);
			int size = (int)fin.tellg();
			string buffer(size, ' ');
			fin.seekg(0, ios::beg);
			fin.read(&buffer[0], size);
			fin.clear();
			tokens.insert(path, buffer, this);
		}
	}
}

string Binder::findWorkingDir() const {
	return std::filesystem::current_path();
}

string Binder::findProjectRoot(string workingDir) const {
	if (workingDir.empty()) {
		workingDir = findWorkingDir();
	}
	std::filesystem::path search = std::filesystem::absolute(workingDir);
	while (not search.empty()) {
		if (std::filesystem::exists(search / "lm.mod")) {
			return search.string();
		} else if (search.parent_path() == search) {
			printf("error: unable to find project root\n");
			return "";
		}
		search = search.parent_path();
	}
	printf("error: unable to find project root\n");
	return "";
}

/*void Binder::addIncludePath(string path) {
	if (path.size() > 0u and path.back() != '/' and path.back() != '\\') {
		path.push_back('/');
	}

	if ((path.size() < 3u or path.substr(1, 2) != ":\\") and (path.size() < 1u or path[0] != '/')) {
		path = projectRoot + path;
	}

	includePath.push_back(path);
}*/

bool import_declaration(vector<Instance> &result, const Program &prgm, int index, const parse_ucs::declaration &syntax) {
	TypeId type = prgm.findType(index, syntax.type.names);
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

Decl import_prototype(const Program &prgm, int index, const parse_ucs::prototype &syntax, TypeId recvType) {
	TypeId retType = prgm.findType(index, syntax.ret.names);
	vector<Instance> args;
	for (auto i = syntax.args.begin(); i != syntax.args.end(); i++) {
		import_declaration(args, prgm, index, *i);
	}

	return Decl(syntax.name, args, retType, recvType);
}

bool Binder::define(vector<string> typeName, string name, vector<int> size, ucs::Netlist nets) {
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
}

void Binder::loadSymbols(int index, const parse_ucs::source &syntax) {
	currModule = index;
	prgm.mods[currModule].name = syntax.name;
	for (auto i = syntax.types.begin(); i != syntax.types.end(); i++) {
		int recvType = prgm.mods[currModule].createType(Type::typeOf(i->name));
	}
}

void Binder::loadModule(int index, const parse_ucs::source &syntax) {
	currModule = index;
	//cout << syntax.to_string() << endl;
	for (auto i = syntax.types.begin(); i != syntax.types.end(); i++) {
		TypeId recvType = prgm.findType(currModule, {i->name});
		for (auto j = i->members.begin(); j != i->members.end(); j++) {
			import_declaration(prgm.typeAt(recvType).members, prgm, currModule, *j);
		}

		for (auto j = i->protocols.begin(); j != i->protocols.end(); j++) {
			prgm.typeAt(recvType).methods.push_back(import_prototype(prgm, currModule, *j, recvType));
		}
	}

	for (auto i = syntax.funcs.begin(); i != syntax.funcs.end(); i++) {
		int kind = Term::findDialect(i->lang);
		TypeId recvType;
		if (not i->recv.empty()) {
			recvType = prgm.findType(currModule, {i->recv});
			if (not recvType.defined()) {
				printf("error: type not defined '%s'\n", i->recv.c_str());
				continue;
			}
		}

		TypeId retType;
		if (i->ret.valid) {
			retType = prgm.findType(currModule, i->ret.names);
			if (not retType.defined()) {
				printf("error: type not defined '%s'\n", i->ret.to_string().c_str());
				continue;
			}
		}

		vector<Instance> args;
		for (auto j = i->args.begin(); j != i->args.end(); j++) {
			import_declaration(args, prgm, currModule, *j);
		}

		currTerm = prgm.mods[currModule].createTerm(Term::procOf(kind, i->name, args, retType, recvType));

		if (kind >= 0) {
			prgm.mods[currModule].terms[currTerm].def = Term::dialects[kind].factory(i->body, nullptr);
		}
	}
}

void Binder::load(string path) {
	if (path.empty()) {
		path = findProjectRoot() + "/top.wv";
	}

	size_t dot = path.find_last_of(".");
	string ext = "";
	if (dot != string::npos) {
		ext = path.substr(dot+1);
	}

	int kind = findDialect(ext);
	if (kind == Binder::NONE) {
		printf("error: unrecognized dialect '%s'\n", ext.c_str());
		return;
	}

	dialects[kind].factory(*this, path);
}

}
